// vim: set fileencoding=utf-8 noet :

/* timetag-tools - Tools for UMass FPGA timetagger
 *
 * Copyright © 2010 Ben Gamari
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/ .
 *
 * Author: Ben Gamari <bgamari@physics.umass.edu>
 */


#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <libusb.h>

#include <sys/time.h>
#include <sys/resource.h>

#ifdef WITH_DOMAIN_SOCKET
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#endif

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>

#include "timetagger.h"
#include "record_format.h"

#define VENDOR_ID 0x04b4
#define PRODUCT_ID 0x1004

namespace io = boost::iostreams;
typedef io::stream<io::file_descriptor> fdstream;

// TODO: This shouldn't be global
static unsigned int written = 0;

struct data_cb : timetagger::data_cb_t {
	unsigned int& written;
	data_cb(unsigned int& written) : written(written) { }
	void operator()(const uint8_t* buffer, size_t length) {
		/*
		 * HACK: Ignore first 8 records
		 * Hardware returns old records at beginning of acquisition
		 */
		int skip = 0;
		if (written < 8*RECORD_LENGTH)
			skip += 8*RECORD_LENGTH;

		write(1, buffer+skip, length-skip);
		written += length;
	}
};

/*
 * Return whether to stop
 */
static bool handle_command(timetagger& t, std::string line, std::ostream& ctrl_out)
{
	using boost::lexical_cast;
	std::vector<std::string> tokens;
	boost::split(tokens, line, boost::is_any_of("\t "));
	if (tokens.size() == 0) {
		ctrl_out << "error: no command\n";
		return false;
	}

	struct command {
		std::string name;
		unsigned int n_args;
		boost::function<void ()> f;
		const char* description;
		const char* args;
	};
	std::vector<command> commands = {
		{"start_capture", 0,
			[&]() { t.start_capture(); },
			"Start the timetagging engine"
		},
		{"stop_capture", 0,
			[&]() { t.stop_capture(); },
			"Stop the timetagging engine"
		},
		{"reset", 0,
			[&]() { t.reset(); },
			"Perform hardware reset"
		},
		{"set_send_window", 1,
			[&]() {
				int records = lexical_cast<int>(tokens[1]);
				t.set_send_window(records);
			},
			"Set the USB send window size",
			"SIZE"
		},
		{"set_strobe", 2,
			[&]() {
				int channel = lexical_cast<int>(tokens[1]);
				bool enabled = lexical_cast<bool>(tokens[2]);
				t.set_strobe_channel(channel, enabled);
			},
			"Enable/disable a strobe channel",
			"CHAN ENABLED"
		},
		{"set_delta", 2,
			[&]() {
				int channel = lexical_cast<int>(tokens[1]);
				bool enabled = lexical_cast<bool>(tokens[2]);
				t.set_delta_channel(channel, enabled);
			},
			"Enable/disable a strobe channel",
			"CHAN ENABLED"
		},
		{"version", 0,
			[&]() { ctrl_out << t.get_version() << "\n"; },
			"Display hardware version"
		},
		{"clockrate", 0,
			[&]() { ctrl_out << t.get_clockrate() << "\n"; },
			"Display hardware acquisition clockrate"
		},
		{"reset_counter", 0,
			[&]() { t.reset_counter(); },
			"Reset timetag counter"
		},
		{"record_count", 0,
			[&]() { ctrl_out << t.get_record_count() << "\n"; },
			"Display current record count"
		},
		{"lost_record_count", 0,
			[&]() { ctrl_out << t.get_lost_record_count() << "\n"; },
			"Display current lost record count"
		},
		{"seq_clockrate", 0,
			[&]() { ctrl_out << t.get_seq_clockrate() << "\n"; },
			"Display sequencer clockrate"
		},
		{"start_seq", 0,
			[&]() { t.set_global_sequencer_operate(true); },
			"Start sequencer"
		},
		{"stop_seq", 0,
			[&]() { t.set_global_sequencer_operate(false); },
			"Stop sequencer"
		},
		{"reset_seq", 0,
			[&]() { t.reset_sequencer(); },
			"Return sequencer outputs to initial states"
		},
		{"seqchan_operate", 2,
			[&]() {
				int channel = lexical_cast<int>(tokens[1]);
				bool enabled = lexical_cast<bool>(tokens[2]);
				t.set_seqchannel_operate(channel, enabled);
			},
			"Enable/disable sequencer channel",
			"CHAN ENABLED"
		},
		{"seqchan_config", 5,
			[&]() {
				int channel = lexical_cast<int>(tokens[1]);
				bool initial_state = lexical_cast<bool>(tokens[2]);
				int initial_count = lexical_cast<int>(tokens[3]);
				int low_count = lexical_cast<int>(tokens[4]);
				int high_count = lexical_cast<int>(tokens[5]);
				t.config_seqchannel(channel, initial_state,
						initial_count, low_count, high_count);
			},
			"Configure sequencer channel",
			"CHAN INITIAL_STATE INITIAL_COUNT LOW_COUNT HIGH_COUNT"
		},
	};

	std::string cmd = tokens[0];
	if (cmd == "quit")
		return true;
	if (cmd == "help") {
		for (auto c=commands.begin(); c != commands.end(); c++)
			ctrl_out << boost::format("%-10s\t%s\n     %s\n") % c->name % c->args % c->description;
		ctrl_out << "\n";
		return false;
	}
	for (auto c=commands.begin(); c != commands.end(); c++) {
		if (c->name == cmd) {
			if (tokens.size() != c->n_args+1) {
				ctrl_out << boost::format("error: invalid command (expects %d arguments)\n") % c->n_args;
				return false;
			}
			c->f();
			return false;
		}
	}

	ctrl_out << "error: unknown command\n";
	return false;
}

template<typename InputStream, typename OutputStream>
static void read_loop(timetagger& t, boost::mutex& mutex,
		InputStream& ctrl_in, OutputStream& ctrl_out)
{
	ctrl_out << "timetag_acquire\n";

	bool stop = false;
	while (!ctrl_in.eof() && !stop) {
		std::string line;
		ctrl_out << "ready\n";
		std::getline(std::cin, line);
		boost::mutex::scoped_lock lock(mutex);
		stop = handle_command(t, line, ctrl_out);
	}
}

int main(int argc, char** argv)
{
	libusb_context* ctx;
	libusb_device_handle* dev;

	// Disable output buffering
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	// Try bumping up our priority
	if (setpriority(PRIO_PROCESS, 0, -10))
		fprintf(stderr, "Warning: Priority elevation failed.\n");

	libusb_init(&ctx);
	dev = libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID);
	if (!dev) {
		fprintf(stderr, "Failed to open device.\n");
		exit(1);
	}

	data_cb cb(written);
	boost::mutex dev_mutex;
	timetagger t(dev, cb);
	t.reset();
	t.start_readout();

#ifdef WITH_DOMAIN_SOCKET
        if (argc > 1) {
		char* ctrl_sock_name = argv[1];
                int socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
                if (socket_fd < 0) {
                        fprintf(stderr, "Error opening control fd: %s\n", strerror(errno));
			return 1;
                }
		
		unlink(ctrl_sock_name);
		struct sockaddr_un address;
		size_t address_len;
		address.sun_family = AF_UNIX;
		address_len = sizeof(address.sun_family) +
			sprintf(address.sun_path, "%s", ctrl_sock_name);

		if (bind(socket_fd, (struct sockaddr*) &address, address_len) != 0) {
			fprintf(stderr, "Error binding socket: %s\n", strerror(errno));
			return 1;
		}

		if (listen(socket_fd, 5) != 0) {
			fprintf(stderr, "Error listening on socket: %s\n", strerror(errno));
			return 1;
		}
	
		fprintf(stderr, "Listening on socket\n");
		int conn_fd;
		std::vector<boost::thread*> threads;
		while ((conn_fd = accept(socket_fd,
					(struct sockaddr*) &address,
					(socklen_t*) &address_len)) > -1)
		{
			auto fds = io::file_descriptor(socket_fd, io::close_handle);
			io::stream<io::file_descriptor> ctrl(fds);
			threads.push_back(new boost::thread([&](){ read_loop(t, dev_mutex, ctrl, ctrl);} ));
		}
		t.stop_readout();
		libusb_close(dev);
		return 0;
        }
#endif

	read_loop(t, dev_mutex, std::cin, std::cerr);
	t.stop_readout();
	libusb_close(dev);
	return 0;
}


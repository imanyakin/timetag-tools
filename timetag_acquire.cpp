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
#include <signal.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef WITH_DOMAIN_SOCKET
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <thread>

#include "timetagger.h"
#include "record_format.h"

#define VENDOR_ID 0x04b4
#define PRODUCT_ID 0x1004

// TODO: This shouldn't be global
static unsigned int written = 0;

struct output_fd {
	int fd;
	std::string name;
	bool needs_close;
	unsigned int lost_records;
};
static std::list<output_fd> output_fds;

struct data_cb : timetagger::data_cb_t {
	unsigned int& written;
	data_cb(unsigned int& written) : written(written) { }

	void operator()(const uint8_t* buffer, size_t length) {
		for (auto fd=output_fds.begin(); fd != output_fds.end(); fd++) {
			int ret = write(fd->fd, buffer, length);
			if (ret < 0 && errno == EAGAIN)
				fd->lost_records += length / RECORD_LENGTH;
		}
		written += length;
	}
};

static int recv_fd(int socket)
{
	struct msghdr message;
	struct iovec iov[1];
	char cmsgbuf[CMSG_SPACE(sizeof(int))];
	char buffer[255];

	iov[0].iov_base = buffer;
	iov[0].iov_len = sizeof(buffer);

	message.msg_iov = iov;
	message.msg_iovlen = 1;

	message.msg_name = NULL;
	message.msg_namelen = 0;
	message.msg_control = cmsgbuf;
	message.msg_controllen = CMSG_SPACE(sizeof(int));
	message.msg_flags = 0;

	if (recvmsg(socket, &message, 0) < 0) {
		fprintf(stderr, "Failed to receive fd: %s\n", strerror(errno));
		return -1;
	}

	for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&message);
			cmsg != NULL;
			cmsg = CMSG_NXTHDR(&message, cmsg))
	{
		if ((cmsg->cmsg_level == SOL_SOCKET) &&
				(cmsg->cmsg_type == SCM_RIGHTS))
		{
			return ((int*)CMSG_DATA(cmsg))[0];
		}
	}

	return -1;
}

/*
 * Return whether to stop
 */
static bool handle_command(timetagger& t, std::string line, FILE* ctrl_out, int sock_fd=-1)
{
	using boost::lexical_cast;
	std::vector<std::string> tokens;
	boost::split(tokens, line, boost::is_any_of("\t "));
	if (tokens.size() == 0) {
		fprintf(ctrl_out, "error: no command\n");
		return false;
	}

	struct command {
		std::string name;
		unsigned int n_args;
		boost::function<void ()> f;
		std::string description;
		std::string args;
	};
	std::vector<command> commands = {
		{"add_output", 1,
			[&]() {
				std::string name = tokens[1];
				if (sock_fd == -1) {
					fprintf(ctrl_out, "error: Command only support in socket mode\n");
					return;
				}
				int fd = recv_fd(sock_fd);
				if (fd < 0) {
					fprintf(ctrl_out, "error: Error receiving fd (%s)\n",
							strerror(errno));
					return;
				}
				// Make sure a stalled fd doesn't cause us to lose samples
				fcntl(fd, F_SETFL, O_NONBLOCK);
				output_fd a = { fd, name, true };
				output_fds.push_back(a);
			},
			"Add an output (expects to be sent an fd over domain socket)",
			"NAME"
		},
		{"remove_output", 1,
			[&]() {
				std::string name = tokens[1];
				for (auto fd=output_fds.begin(); fd != output_fds.end(); fd++) {
					if (fd->name == name && fd->needs_close)
						close(fd->fd);
				}
				output_fds.remove_if([&](const output_fd& fd){ return fd.name == name; });
			},
			"Remove an output",
			"FILENAME"
		},
		{"list_outputs", 0,
			[&]() {
				for (auto fd=output_fds.begin(); fd != output_fds.end(); fd++)
					fprintf(ctrl_out, "= %15s\t%d\t%d\n",
							fd->name.c_str(), fd->fd, fd->lost_records);
			},
			"List outputs"
		},
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
		{"flush_fifo", 0,
			[&]() { t.flush_fifo(); },
			"Flush the hardware's record FIFO"
		},
		{"set_send_window", 1,
			[&]() {
				int records = lexical_cast<int>(tokens[1]);
				t.set_send_window(records);
			},
			"Set the USB send window size",
			"SIZE"
		},
		{"strobe_operate", 2,
			[&]() {
				int channel = lexical_cast<int>(tokens[1]);
				bool enabled = lexical_cast<bool>(tokens[2]);
				t.set_strobe_operate(channel, enabled);
			},
			"Enable/disable a strobe channel",
			"CHAN ENABLED"
		},
		{"strobe_operate?", 1,
			[&]() {
				int channel = lexical_cast<int>(tokens[1]);
				fprintf(ctrl_out, "= %d\n", t.get_strobe_operate(channel));
			},
			"Display operational state of strobe channel",
			"CHAN"
		},
		{"delta_operate", 2,
			[&]() {
				int channel = lexical_cast<int>(tokens[1]);
				bool enabled = lexical_cast<bool>(tokens[2]);
				t.set_delta_operate(channel, enabled);
			},
			"Enable/disable a strobe channel",
			"CHAN ENABLED"
		},
		{"delta_operate?", 1,
			[&]() {
				int channel = lexical_cast<int>(tokens[1]);
				fprintf(ctrl_out, "= %d\n", t.get_delta_operate(channel));
			},
			"Display operational state of delta channel",
			"CHAN"
		},
		{"version?", 0,
			[&]() { fprintf(ctrl_out, "= %u\n", t.get_version()); },
			"Display hardware version"
		},
		{"clockrate?", 0,
			[&]() { fprintf(ctrl_out, "= %u\n", t.get_clockrate()); },
			"Display hardware acquisition clockrate"
		},
		{"reset_counter", 0,
			[&]() { t.reset_counter(); },
			"Reset timetag counter"
		},
		{"record_count?", 0,
			[&]() { fprintf(ctrl_out, "= %u\n", t.get_record_count()); },
			"Display current record count"
		},
		{"lost_record_count?", 0,
			[&]() { fprintf(ctrl_out, "= %u\n", t.get_lost_record_count()); },
			"Display current lost record count"
		},
		{"seq_clockrate?", 0,
			[&]() { fprintf(ctrl_out, "= %u\n", t.get_seq_clockrate()); },
			"Display sequencer clockrate"
		},
		{"seq_operate", 1,
			[&]() { 
				bool enabled = lexical_cast<bool>(tokens[1]);
				t.set_global_sequencer_operate(enabled);
			},
			"Enable/disable sequencer",
			"CHAN"
		},
		{"seq_operate?", 0,
			[&]() {
				fprintf(ctrl_out, "= %d\n", t.get_global_sequencer_operate());
			},
			"Get operational state of sequencer",
		},
		{"reset_seq", 0,
			[&]() { t.reset_sequencer(); },
			"Return sequencer outputs to initial states"
		},
		{"seqchan_operate", 2,
			[&]() {
				int channel = lexical_cast<int>(tokens[1]);
				bool enabled = lexical_cast<bool>(tokens[2]);
				t.set_seqchan_operate(channel, enabled);
			},
			"Enable/disable sequencer channel",
			"CHAN ENABLED"
		},
		{"seqchan_operate?", 1,
			[&]() {
				int channel = lexical_cast<int>(tokens[1]);
				fprintf(ctrl_out, "= %d\n", t.get_seqchan_operate(channel));
			},
			"Get operational state of sequencer channel",
			"CHAN"
		},
		{"seqchan_config", 5,
			[&]() {
				int channel = lexical_cast<int>(tokens[1]);
				bool initial_state = lexical_cast<bool>(tokens[2]);
				int initial_count = lexical_cast<int>(tokens[3]);
				int low_count = lexical_cast<int>(tokens[4]);
				int high_count = lexical_cast<int>(tokens[5]);
				t.set_seqchan_initial_state(channel, initial_state);
				t.set_seqchan_initial_count(channel, initial_count);
				t.set_seqchan_low_count(channel, low_count);
				t.set_seqchan_high_count(channel, high_count);
			},
			"Configure sequencer channel",
			"CHAN INITIAL_STATE INITIAL_COUNT LOW_COUNT HIGH_COUNT"
		},
		{"seqchan_initial_state?", 1,
			[&]() {
				int channel = lexical_cast<int>(tokens[1]);
				fprintf(ctrl_out, "= %u\n", t.get_seqchan_initial_state(channel));
			},
			"Get initial state of sequencer channel",
			"CHAN"
		},
		{"seqchan_initial_count?", 1,
			[&]() {
				int channel = lexical_cast<int>(tokens[1]);
				fprintf(ctrl_out, "= %u\n", t.get_seqchan_initial_count(channel));
			},
			"Get initial count of sequencer channel",
			"CHAN"
		},
		{"seqchan_low_count?", 1,
			[&]() {
				int channel = lexical_cast<int>(tokens[1]);
				fprintf(ctrl_out, "= %u\n", t.get_seqchan_low_count(channel));
			},
			"Get low count of sequencer channel",
			"CHAN"
		},
		{"seqchan_high_count?", 1,
			[&]() {
				int channel = lexical_cast<int>(tokens[1]);
				fprintf(ctrl_out, "= %u\n", t.get_seqchan_high_count(channel));
			},
			"Get high count of sequencer channel",
			"CHAN"
		},
	};

	std::string cmd = tokens[0];
	if (cmd == "quit")
		return true;
	if (cmd == "help") {
		for (auto c=commands.begin(); c != commands.end(); c++)
			fprintf(ctrl_out, "= %-10s\t%s\n     %s\n",
					c->name.c_str(), c->args.c_str(), c->description.c_str());
		fprintf(ctrl_out, "= \n");
		return false;
	}
	for (auto c=commands.begin(); c != commands.end(); c++) {
		if (c->name == cmd) {
			if (tokens.size() != c->n_args+1) {
				fprintf(ctrl_out, "error: invalid command (expects %d arguments)\n", c->n_args);
				return false;
			}
			c->f();
			return false;
		}
	}

	fprintf(ctrl_out, "error: unknown command\n");
	return false;
}

static void read_loop(timetagger& t, boost::mutex& mutex,
		FILE* ctrl_in, FILE* ctrl_out, int sock_fd=-1)
{
	char* buf = new char[255];
	bool stop = false;
	while (!ferror(ctrl_in) && !stop) {
		size_t n = 255;
		fprintf(ctrl_out, "ready\n");
		if (getline(&buf, &n, ctrl_in) == -1) break;
		boost::mutex::scoped_lock lock(mutex);
		std::string line(buf);
		line = line.substr(0, line.length()-1);
		stop = handle_command(t, line, ctrl_out, sock_fd);
	}
	delete [] buf;
	fclose(ctrl_out);
	if (ctrl_out != ctrl_in)
		fclose(ctrl_in);
}

int main(int argc, char** argv)
{
	libusb_context* ctx;
	libusb_device_handle* dev;

	// Handle SIGPIPE somewhat reasonably
	signal(SIGPIPE, SIG_IGN);
	
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

		if (chmod(address.sun_path, 0666) != 0) {
			fprintf(stderr, "Error chmodding socket: %s\n", strerror(errno));
			return 1;
		}

		if (listen(socket_fd, 5) != 0) {
			fprintf(stderr, "Error listening on socket: %s\n", strerror(errno));
			return 1;
		}
	
		fprintf(stderr, "Listening on socket\n");
		int conn_fd;
		std::vector<std::thread*> threads;
		while ((conn_fd = accept(socket_fd,
					(struct sockaddr*) &address,
					(socklen_t*) &address_len)) > -1)
		{
			FILE* conn = fdopen(conn_fd, "r+");
			threads.push_back(new std::thread([&](){ read_loop(t, dev_mutex, conn, conn, conn_fd); }));
		}
		fprintf(stderr, "Cleaning up...\n");
		for (auto thrd=threads.begin(); thrd != threads.end(); thrd++)
			(*thrd)->join();
		t.stop_readout();
		libusb_close(dev);
		return 0;
        }
#endif

	output_fd a = { 1, "stdout", false };
	output_fds.push_back(a);
	read_loop(t, dev_mutex, stdin, stderr);
	t.stop_readout();
	libusb_close(dev);
	return 0;
}


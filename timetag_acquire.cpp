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

#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>

#include "timetagger.h"
#include "record_format.h"

#define VENDOR_ID 0x04b4
#define PRODUCT_ID 0x1004


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
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep("\t ");
	tokenizer tokens(line, sep);
	if (tokens.begin() == tokens.end())
		return false;

	auto tok = tokens.begin();
	std::string cmd = *tok;
	tok++;

	if (cmd == "start_capture") {
		t.start_capture();
	} else if (cmd == "stop_capture") {
		t.stop_capture();
	} else if (cmd == "reset") {
		t.reset();
	} else if (cmd == "set_send_window") {
		int records = lexical_cast<int>(*tok);
		t.set_send_window(records);
	} else if (cmd == "set_strobe") {
		int channel = lexical_cast<int>(*tok); tok++;
		bool enabled = lexical_cast<bool>(*tok);
		t.set_strobe_channel(channel, enabled);
	} else if (cmd == "set_delta") {
		int channel = lexical_cast<int>(*tok); tok++;
		bool enabled = lexical_cast<bool>(*tok);
		t.set_delta_channel(channel, enabled);
	} else if (cmd == "version") {
		ctrl_out << t.get_version() << "\n";
	} else if (cmd == "clockrate") {
		ctrl_out << t.get_clockrate() << "\n";
	} else if (cmd == "reset_counter") {
		t.reset_counter();
	} else if (cmd == "record_count") {
		ctrl_out << t.get_record_count() << "\n";
	} else if (cmd == "lost_record_count") {
		ctrl_out << t.get_lost_record_count() << "\n";
	} else if (cmd == "quit" || cmd == "exit") {
		return true;
	} else
		ctrl_out << "error: invalid command\n";

	return false;
}

static void read_loop(timetagger& t, std::istream& ctrl_in, std::ostream& ctrl_out)
{
	ctrl_out << "timetag_acquire\n";

	bool stop = false;
	while (!ctrl_in.eof() && !stop) {
		std::string line;
		ctrl_out << "ready\n";
		std::getline(std::cin, line);
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
	timetagger t(dev, cb);
	t.reset();
	t.start_readout();

#ifdef WITH_DOMAIN_SOCKET
	int socket_fd;
        if (argc > 1) {
		char* ctrl_sock_name = argv[1];
                socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
                if (socket_fd < 0) {
                        fprintf(stderr, "Error opening control fd: %s\n", strerror(errno));
			return 1;
                }
		
		unlink(ctrl_sock_name);
		struct sockaddr_un address;
		size_t address_len;
		address.sun_family = AF_UNIX;
		address_length = sizeof(address.sun_family) +
			sprintf(address.sun_path, ctrl_sock_name);

		if (bind(socket_fd, (struct sockaddr*) &address, address_len) != 0) {
			fprintf(stderr, "Error binding socket: %s\n", strerror(errno));
			return 1;
		}

		if (listen(socket_fd, 5) != 0) {
			fprintf(stderr, "Error listening on socket: %s\n", strerror(errno));
			return 1;
		}
	
		int conn_fd;
		while ((conn_fd = accept(socket_fd,
					(struct sockaddr*) &address,
					&address_length)) > -1)
		{
			std::ifstream ctrl_in(socket_fd);
			std::ofstream ctrl_out(socket_fd);
			read_loop(t, ctrl_in, ctrl_out);
		}
		t.stop_readout();
		libusb_close(dev);
		return 0;
        }
#endif

	setvbuf(stderr, NULL, _IONBF, 0);
	read_loop(t, std::cin, std::cerr);
	t.stop_readout();
	libusb_close(dev);
	return 0;
}


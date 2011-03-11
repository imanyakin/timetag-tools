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
#include <libusb.h>

#include <sys/time.h>
#include <sys/resource.h>

#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>

#include "timetagger.h"
#include "record_format.h"

#define VENDOR_ID 0x04b4
#define PRODUCT_ID 0x1004


// TODO: This shouldn't be global
static unsigned int written = 0;

FILE* ctl_fd = stderr;

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
static bool handle_command(timetagger& t, std::string line)
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
	} else if (cmd == "version") {
		fprintf(ctl_fd, "%d\n", t.get_version());
	} else if (cmd == "clockrate") {
		fprintf(ctl_fd, "%d\n", t.get_clockrate());
	} else if (cmd == "reset_counter") {
		t.reset_counter();
	} else if (cmd == "record_count") {
		fprintf(ctl_fd, "%d\n", t.get_record_count());
	} else if (cmd == "lost_record_count") {
		fprintf(ctl_fd, "%d\n", t.get_lost_record_count());
	} else if (cmd == "quit" || cmd == "exit") {
		return true;
	} else
		fprintf(ctl_fd, "Invalid command\n");

	return false;
}

static void read_loop(timetagger& t)
{
	t.start_readout();

	// Command loop
	bool stop = false;
	while (!std::cin.eof() && !stop) {
		std::string line;
		fprintf(ctl_fd, "ready\n");
		std::getline(std::cin, line);
		stop = handle_command(t, line);
	}

	t.stop_readout();
}

int main(int argc, char** argv)
{
	libusb_context* ctx;
	libusb_device_handle* dev;

        // Use control fd if provided
        if (argc > 1) {
                ctl_fd = fdopen(atoi(argv[1]), "w");
                if (!ctl_fd) {
                        fprintf(stderr, "Error opening control fd: %s\n", strerror(errno));
                        exit(1);
                }
        }

	// Disable output buffering
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(ctl_fd, NULL, _IONBF, 0);

	// Try bumping up our priority
	if (setpriority(PRIO_PROCESS, 0, -10))
		fprintf(stderr, "Warning: Priority elevation failed.\n");

	fprintf(ctl_fd, "timetag_acquire\n");

	libusb_init(&ctx);
	dev = libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID);
	if (!dev) {
		fprintf(stderr, "Failed to open device.\n");
		exit(1);
	}

	data_cb cb(written);
	timetagger t(dev, cb);
	t.reset();
	read_loop(t);

	libusb_close(dev);
}


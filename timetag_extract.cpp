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

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <boost/format.hpp>

#include "record.h"

using std::string;

/*
 * Outputs binary timestamps from timetag data stream
 * Usage:
 *   timetag_extract [input-file]
 *
 *   Generates timestamp files for all non-empty channels
 *
 * Input:
 *   A binary photon stream
 *
 * Output:
 *   strobe channels: uint64 binary timestamp data
 *   delta channels: Binary records consisting of a uint64 timestamp followed
 *                   by a 1 byte long state
 *
 */

string root;
std::array<FILE*, 4> strobe_out, delta_out;
uint64_t first_delta_time;
std::bitset<4> delta_states;
bool first_delta;

void process_record(record& r) {
	std::bitset<4> channels = r.get_channels();
	uint64_t time = r.get_time();
	if (r.get_type() == record::type::STROBE) {
		for (int i=0; i<4; i++) {
			if (!channels[i]) continue;
			if (strobe_out[i] == NULL) {
				string name = str(boost::format("%s.strobe%d.times")
						% root % (i+1));
				strobe_out[i] = fopen(name.c_str(), "w");
			}
			fwrite((char*)&time, 8, 1, strobe_out[i]);
		}
	} else {
		if (first_delta) {
			first_delta_time = time;
			for (int i=0; i<4; i++)
				delta_states = channels;
			first_delta = false;
			return;
		}
		for (int i=0; i<4; i++) {
			bool new_state = channels[i];
			bool old_state = delta_states[i];
			if (new_state == old_state) continue;
			if (delta_out[i] == NULL) {
				string name = (boost::format("%s.delta%d.times")
						% root % (i+1)).str();
				delta_out[i] = fopen(name.c_str(), "w");
				fwrite((char*)&first_delta_time, 8, 1, delta_out[i]);
				fwrite((char*)&old_state, 1, 1, delta_out[i]);
			}
			fwrite((char*)&time, 8, 1, delta_out[i]);
			fwrite((char*)&new_state, 1, 1, delta_out[i]);
			delta_states[i] = new_state;
		}
	}
}

int main(int argc, char** argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s [input file]\n", argv[0]);
		exit(0);
	}

	string name = string(argv[1]);
	root = name.substr(0, name.find_last_of("."));
	FILE* infd = fopen(argv[1], "r");
	record_stream stream(infd);

	first_delta = true;
	while (true) {
		try {
			record r = stream.get_record();
			process_record(r);
		} catch (end_stream e) { break; }
	}

	for (int i=0; i<4; i++) {
		if (strobe_out[i])
			fclose(strobe_out[i]);
		if (delta_out[i])
			fclose(delta_out[i]);
	}

	return 0;
}


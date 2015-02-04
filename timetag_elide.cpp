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

/*
 * This tool elides delta events that don't bracket a strobe event, greatly reducing
 * file sizes in ALEX-type experiments without loss of information.
 * 
 * In particular, we first keep the first 1000 delta events to ensure
 * that the excitation periods can be properly recovered during
 * analysis. After this initial period, we only keep delta events that
 * come before or after a strobe event.
 *
 */

#include <vector>
#include <iostream>
#include <boost/program_options.hpp>
#include "record.h"

namespace po = boost::program_options;

int main(int argc, char** argv) {
	unsigned int drop_wraps = 0;
	record_stream stream(stdin, drop_wraps);
	try {
		bool last_delta_valid = false;
		record last_delta(0,0);
		unsigned int i=0;
		while (true) {
			record r = stream.get_record();
			write_record(stdout, r);
			if (r.get_type() == record::DELTA) {
				// Always keep the first 1000 delta events
				last_delta = r;
				last_delta_valid = true;
				i++;
				if (i > 1000)
					break;
			}
		}

		bool write_next_delta = false;
		while (true) {
			record r = stream.get_record();
			if (r.get_type() == record::STROBE) {
				if (last_delta_valid) {
					write_record(stdout, last_delta);
					last_delta_valid = false;
				}
				write_record(stdout, r);
				write_next_delta = true;
			} else {
				if (write_next_delta) {
					write_record(stdout, r);
					write_next_delta = false;
				} else {
					last_delta_valid = true;
					last_delta = r;
				}
			}
			
		}
	} catch (end_stream e) { }
}

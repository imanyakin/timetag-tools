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
#include <vector>
#include <iostream>
#include <fstream>
#include <endian.h>

#include "record.h"

/*
 *
 * Outputs binary timestamps from timetag data stream
 * Usage:
 *   extract_timestamps [chan1] [chan2] [chan3] [chan4]
 *
 *   Where chan* are output filenames for the respective channel timestamps
 *
 * Input:
 *   A binary photon stream
 *
 * Output:
 *   uint64 binary timestamp data
 *
 */

int main(int argc, char** argv) {
        std::vector<std::ofstream*> ofs(4, NULL);
        record_stream stream(0);

        if (argc > 4) {
                std::cerr << "Incorrect number of arguments\n";
                return 1;
        }

        for (int i=0; i<argc; i++)
                ofs[i] = new std::ofstream(argv[i+1]);

        while (true) {
		try {
			record r = stream.get_record();
                        if (r.get_type() == record::type::DELTA) continue;
                        std::bitset<4> channels = r.get_channels();
                        uint64_t time = r.get_time();
                        for (int i=0; i<4; i++)
                                if (channels[i] && ofs[i]) ofs[i]->write((char*)&time, 8);
		} catch (end_stream e) { break; }
        }

	for (int i=0; i<argc; i++)
		delete ofs[i];

        return 0;
}


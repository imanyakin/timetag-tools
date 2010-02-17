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
#include <endian.h>

#include "record.h"

/*
 *
 * Decodes binary records to human readable photon stream.
 * Usage:
 *   dump_photons
 *
 * Input:
 *   A binary photon stream
 *
 * Output:
 *   TIME       CHAN    OUTPUTS...
 *
 */

struct input_channel {
        int chan_n;

        input_channel(int chan_n) : chan_n(chan_n) { }
};

struct output_channel {
        int chan_n;
        bool state;

        output_channel(int chan_n) :
                chan_n(chan_n), state(false) { }
};

void dump_photon(record& r, std::vector<input_channel>& inputs, std::vector<output_channel>& outputs) {
	std::bitset<4> channels = r.get_channels();
	uint64_t time = r.get_time();

	if (r.get_type() == record::type::DELTA) {
		// Delta record
		for (auto chan=outputs.begin(); chan != outputs.end(); chan++)
			chan->state = channels[chan->chan_n];
	} else  {
		// Strobe record
		for (auto chan=inputs.begin(); chan != inputs.end(); chan++) {
			if (!channels[chan->chan_n]) continue;
			printf("%11llu\t%d\t", (long long unsigned) time, chan->chan_n);
			for (auto out=outputs.begin(); out != outputs.end(); out++)
				printf("%d\t", out->state);
			printf("\n");
		}
	}
}

int main(int argc, char** argv) {
        std::vector<output_channel> outputs = {
                output_channel(0),
                output_channel(1),
                output_channel(2),
                output_channel(3)
        };
        std::vector<input_channel> inputs = {
                input_channel(0),
                input_channel(1),
                input_channel(2),
                input_channel(3)
        };
        record_stream stream(0);

        while (true) {
		try {
			record r = stream.get_record();
			dump_photon(r, inputs, outputs);
		} catch (end_stream e) { break; }
        }

        return 0;
}


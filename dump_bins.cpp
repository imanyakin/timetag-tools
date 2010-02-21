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


#include <vector>
#include <cstdio>
#include <unistd.h>
#include "bin_format.h"

/*
 *
 * Formats a stream of bins in human-readable format
 *
 * Usage:
 *   cat bins | dump_bins
 *
 * Input:
 *   A binary bin stream (the output of bin_photons)
 *
 * Output:
 *   A list of records in the following format:
 *   [CHAN]     [BIN_START_TIME]        [BIN_PHOTON_COUNT]	[LOST_COUNT]
 *
 */

int main() {
	while (true) {
		bin_record rec;
		read(0, &rec, sizeof(bin_record));

		printf("%d\t%11llu\t%u\t%u\n",
				rec.chan_n,
				(long long unsigned) rec.start_time,
				rec.count,
				rec.lost);
	}
}



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
#include <unistd.h>
#include <algorithm>
#include <time.h>

#include "record_format.h"

using namespace std;

#define CLOCK_RATE (30e6)
#define DEVIATION 0.7

/*
 *
 * Generates a reasonably realistic photon stream.
 *
 * Usage:
 *   photon_generator [HZ]
 *
 * Where HZ is photon frequency in Hertz.
 *
 * Output:
 *   A binary photon stream.
 *
 */

int main(int argc, char** argv) {
	if (argc <= 1) {
		fprintf(stderr, "Usage: %s [hz]\n", argv[0]);
		return 1;
	}

	const unsigned int period = 1e9 / atoi(argv[1]); // in nanoseconds
	count_t counter = 0;

	setvbuf(stdout, NULL, _IONBF, 0);
	while (true) {
		record_t data;

		// Generate channel mask
		data = rand();
		data = (data << TIME_BITS) & CHANNEL_MASK;
		data |= counter & TIME_MASK;
		data = htobe64(data) >> 16;
		fwrite(&data, RECORD_LENGTH, 1, stdout);

		int dt = period + drand48()*DEVIATION*period;
		timespec ts = { 0, dt };
		nanosleep(&ts, NULL);
		counter += 1e-9 * CLOCK_RATE * dt;
	}
}


#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <algorithm>
#include <time.h>

#include "photon_format.h"

using namespace std;

#define CLOCKRATE (150e6)
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
	if (argc <= 1) return 1;

	const unsigned int period = 1.0 / atoi(argv[1]) * 1e9; // in nanoseconds
	count_t counter = 0;

	setvbuf(stdout, NULL, _IONBF, NULL);
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
		counter += 1e-9 * CLOCKRATE * dt;
	}
}


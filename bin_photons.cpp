#include <vector>
#include <cstdio>
#include <cstdlib>

#include "photon_format.h"

/*
 *
 * Temporally bins a photon stream
 *
 * Usage:
 *   bin_photons [BIN_LENGTH]
 *
 * Where BIN_LENGTH is the length of each bin in counter units.
 *
 * Input:
 *   A binary photon stream
 *
 * Output:
 *   A list of records in the following format:
 *   [CHAN]	[BIN_START_TIME]	[BIN_PHOTON_COUNT]
 *
 * Notes:
 *   We handle wrap-around here by simply keeping all times as 64-bit and
 *   assume that we wrap-around at most once.  With 1 nanosecond clock units,
 *   this gives us 500 years of acquisition time.
 *
 */

struct input_channel {
	int chan_n;
	record_t mask;
	count_t bin_start;
	int count;
	int lost;	// IMPORTANT: This is not a count of lost photons, only
			//            potential sprees of lost photons
	input_channel(int chan_n, record_t mask) : chan_n(chan_n), mask(mask) { }
};

int main(int argc, char** argv) {
	if (argc <= 1) {
		fprintf(stderr, "Usage: %s [bin_length]\n", argv[0]);
		return 1;
	}
	count_t bin_length = atoi(argv[1]);

	std::vector<input_channel> chans = {
		{ input_channel(0, CHAN_1_MASK) },
		{ input_channel(1, CHAN_2_MASK) },
		{ input_channel(2, CHAN_3_MASK) },
		{ input_channel(3, CHAN_4_MASK) },
	};

	// For handling wraparound
	count_t time_offset = 0;

	// Disable buffering
	setvbuf(stdout, NULL, _IONBF, NULL);

	while (true) {
		record_t photon;
		if (fread(&photon, RECORD_LENGTH, 1, stdin) != 1)
			exit(!feof(stdin));

		photon = be64toh(photon) >> 16;
		count_t time = photon & TIME_MASK;

		// Handle wrap-around
		if (photon & TIMER_WRAP_MASK)
			time_offset += (1ULL<<TIME_BITS) - 1;

		time += time_offset;

		for (auto c=chans.begin(); c != chans.end(); c++) {
			if (photon & LOST_SAMPLE_MASK)
				c->lost++;
			if (!(photon & REC_TYPE_MASK) && (photon & c->mask))
				c->count++;

			if (time > (c->bin_start + bin_length)) {
				printf("%d\t%11llu\t%d\t%d\n",
						c->chan_n,
						c->bin_start,
						c->count,
						c->lost);
				c->lost = 0;
				c->count = 0;
				c->bin_start = (time / bin_length) * bin_length;
			}
		}
	}

	return 0;
}


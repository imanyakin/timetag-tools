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
 *   We handle wrap-around here by simply keeping all times as 64 bit and assume
 *   that we wrap-around at most once.
 *   With 1 nanosecond clock units, this gives us 500 years of acquisition time.
 */


struct channel {
	record_t mask;
	count_t bin_start;
	int count;
};

int main(int argc, char** argv) {
	if (argc <= 1) return 1;
	count_t bin_length = atoi(argv[1]);

#define N_CHAN 4
	channel chans[N_CHAN] = {
		{ CHAN_1_MASK },
		{ CHAN_2_MASK },
		{ CHAN_3_MASK },
		{ CHAN_4_MASK },
	};

	// For handling wraparound
	count_t last_time = 0;
	count_t time_offset = 0;
	setvbuf(stdout, NULL, _IONBF, NULL);

	while (true) {
		record_t photon;
		if (fread(&photon, RECORD_LENGTH, 1, stdin) != 1)
			exit(!feof(stdin));
		count_t time = photon & TIME_MASK;

		// Handle wrap-around
		if (time < last_time)
			time_offset += (1ULL<<TIME_BITS) - time;

		last_time = time;
		time += time_offset;

		for (int c=0; c < 4; c++) {
			if (chans[c].mask & photon)
				chans[c].count++;
			if (time > (chans[c].bin_start + bin_length)) {
				printf("%d\t%llu\t%d\n", c+1, chans[c].bin_start, chans[c].count);
				chans[c].count = 0;
				chans[c].bin_start = time;
			}
		}
	}

	return 0;
}


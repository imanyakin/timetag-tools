#include <cstdio>
#include <cstdlib>
#include <endian.h>

#include "photon_format.h"

/*
 *
 * Decodes a binary photon stream to human readable format.
 * Usage:
 *   dump_records
 *
 * Input:
 *   A binary photon stream
 *
 * Output:
 *   REC_NUM	TIME	TYPE	WRAP    LOST    CHANNELS...
 *
 * Where A, B, C, D are flag statuses
 */

int main(int argc, char** argv) {
	unsigned int count = 0;
	setvbuf(stdout, NULL, _IONBF, NULL);
	while (true) {
		record_t photon = 0;
		count++;

		if (fread(&photon, RECORD_LENGTH, 1, stdin) != 1)
			exit(!feof(stdin));

		photon = be64toh(photon) >> 16;
		count_t time = photon & TIME_MASK;

		printf("%u\t%11llu\t%s\t%s\t%s\t%d\t%d\t%d\t%d\n",
				count, time,
				photon & REC_TYPE_MASK ? "DELTA" : "STROBE",
				photon & TIMER_WRAP_MASK ? "WRAP" : "",
				photon & LOST_SAMPLE_MASK ? "LOST" : "",
				(int) (photon & CHAN_0_MASK != 0),
				(int) (photon & CHAN_1_MASK != 0),
				(int) (photon & CHAN_2_MASK != 0),
				(int) (photon & CHAN_3_MASK != 0)
		);
	}

	return 0;
}


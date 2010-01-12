#include <cstdio>
#include <cstdlib>
#include <endian.h>

#include "photon_format.h"

/*
 *
 * Decodes a binary photon stream to human readable format.
 * Usage:
 *   dump_photons
 *
 * Input:
 *   A binary photon stream
 *
 * Output:
 *   REC_NUM	TIME	TYPE	A B C D		WRAP	LOST
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

		printf("%u\t%9llx\t%s\t%s %s %s %s\t%s\t%s\n",
				count, time,
				photon & REC_TYPE_MASK ? "DELTA" : "STROBE",
				photon & CHAN_1_MASK ? "1" : " ",
				photon & CHAN_2_MASK ? "2" : " ",
				photon & CHAN_3_MASK ? "3" : " ",
				photon & CHAN_4_MASK ? "4" : " ",
				photon & TIMER_WRAP_MASK ? "WRAP" : "",
				photon & LOST_SAMPLE_MASK ? "LOST" : ""
		);
	}

	return 0;
}


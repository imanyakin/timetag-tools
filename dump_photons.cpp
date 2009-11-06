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
 *   COUNT_NUM	TIME		A B C D
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

		printf("%u\t%llx\t%s %s %s %s\t%s %s %s %s\t%s\n",
				count, time,
				photon & CHAN_1_MASK ? "1" : " ",
				photon & CHAN_2_MASK ? "2" : " ",
				photon & CHAN_3_MASK ? "3" : " ",
				photon & CHAN_4_MASK ? "4" : " ",
				photon & OUTPUT_1_MASK ? "L1" : "  ",
				photon & OUTPUT_2_MASK ? "L2" : "  ",
				photon & OUTPUT_3_MASK ? "L3" : "  ",
				photon & OUTPUT_4_MASK ? "L4" : "  ",
				photon & LOST_SAMPLE_MASK ? "LOST" : ""
		);
	}

	return 0;
}


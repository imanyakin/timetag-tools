#include <cstdio>
#include <cstdlib>

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
	int count = 0;
	while (true) {
		count++;

		record_t photon;
		if (fread(&photon, RECORD_LENGTH, 1, stdin) != 1)
			exit(!feof(stdin));
		count_t time = photon & TIME_MASK;

		printf("%d\t%lld\t%s %s %s %s\n",
				count, time,
				photon & CHAN_1_MASK ? "1" : "",
				photon & CHAN_2_MASK ? "2" : "",
				photon & CHAN_3_MASK ? "3" : "",
				photon & CHAN_4_MASK ? "4" : "");
	}

	return 0;
}


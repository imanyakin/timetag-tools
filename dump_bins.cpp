#include <vector>
#include <cstdio>
#include <unistd.h>
#include "bin_format.h"

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
 *   [CHAN]     [BIN_START_TIME]        [BIN_PHOTON_COUNT]	[LOST_COUNT]
 *
 * Notes:
 *   We handle wrap-around here by simply keeping all times as 64-bit and
 *   assume that we wrap-around at most once.  With 1 nanosecond clock units,
 *   this gives us 500 years of acquisition time.
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



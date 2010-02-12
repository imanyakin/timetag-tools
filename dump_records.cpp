#include <cstdio>
#include <cstdlib>
#include <endian.h>

#include "record.h"

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

void dump_record(record& r, int count) { 
	uint64_t time = r.get_raw_time();
	std::bitset<4> channels = r.get_channels();
	printf("%u\t%11llu\t%s\t%s\t%s\t%d\t%d\t%d\t%d\n",
			count, 
			(unsigned long long) time,
			r.get_type() == record::type::DELTA ? "DELTA" : "STROBE",
			r.get_wrap_flag() ? "WRAP" : "",
			r.get_lost_flag() ? "LOST" : "",
			(int) (channels[0]),
			(int) (channels[1]),
			(int) (channels[2]),
			(int) (channels[3]) );
}

int main(int argc, char** argv) {
	unsigned int count = 0;
        record_stream stream(0);

	setvbuf(stdout, NULL, _IONBF, NULL);
	while (true) {
		try {
			record r = stream.get_record();
			dump_record(r, count);
			count++;
		} catch (end_stream e) { break; }
	}

	return 0;
}


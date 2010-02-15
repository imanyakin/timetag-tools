#include <cstdint>

/*
 * bin record format
 */
struct bin_record {
	int chan_n;
	uint64_t start_time;
	unsigned int count;
	unsigned int lost;
};


#include <vector>
#include <cstdio>
#include <cstdlib>

#include "record.h"

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

static bool all_zero_bins = false;

struct input_channel {
        int chan_n;
        count_t bin_start;
        unsigned int count;
        unsigned int lost;      // IMPORTANT: This is not a count of lost photons, only
                                //            potential sprees of lost photons
        input_channel(int chan_n) :
                chan_n(chan_n), bin_start(0), count(0), lost(0) { }
};

void print_bin(int chan_n, uint64_t bin_start, unsigned int count, unsigned int lost) {
        printf("%d\t%11llu\t%u\t%u\n",
                        chan_n,
                        (long long unsigned) bin_start,
                        count,
                        lost);
}

void bin_record(std::vector<input_channel>& chans, count_t bin_length, record& r) {
	std::bitset<4> channels = r.get_channels();
	uint64_t time = r.get_time();
	for (auto c=chans.begin(); c != chans.end(); c++) {
		if (time >= (c->bin_start + bin_length)) {
			uint64_t new_bin_start = (time / bin_length) * bin_length;
			
			// First print photons in last bin
			print_bin(c->chan_n, c->bin_start, c->count, c->lost);

			// Then print zero bins
			if (all_zero_bins) {
				for (uint64_t t=c->bin_start+bin_length; t < new_bin_start; t += bin_length)
					print_bin(c->chan_n, t, 0, 0);
			} else {
				// Print bin at beginning of zero run
				if (time >= (c->bin_start+bin_length))
					print_bin(c->chan_n, c->bin_start+bin_length, 0, 0);
				// Then print bin at end of zero run
				if (time >= (c->bin_start+2*bin_length))
					print_bin(c->chan_n, new_bin_start-bin_length, 0, 0);
			}

			// Then start our new bin
			c->lost = 0;
			c->count = 0;
			c->bin_start = new_bin_start;
		}

		if (r.get_lost_flag())
			c->lost++;
		if (r.get_type() == record::type::STROBE && channels[c->chan_n])
			c->count++;
	}
}

int main(int argc, char** argv) {
        if (argc <= 1) {
                fprintf(stderr, "Usage: %s [bin_length]\n", argv[0]);
                return 1;
        }
        count_t bin_length = atoi(argv[1]);

        std::vector<input_channel> chans = {
                { input_channel(0) },
                { input_channel(1) },
                { input_channel(2) },
                { input_channel(3) },
        };
        record_stream stream(0);
        
        /*
         * We throw away the first photon to get the bin start times.
         */
        {
                record r = stream.get_record();
                uint64_t time = r.get_time();
                for (auto c=chans.begin(); c != chans.end(); c++)
                        c->bin_start = (time / bin_length) * bin_length;
        }

        while (true) {
                record r = stream.get_record();
		bin_record(chans, bin_length, r);
        }

        return 0;
}


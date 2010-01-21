#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <endian.h>

#include "record_format.h"

/*
 *
 * Decodes binary records to human readable photon stream.
 * Usage:
 *   dump_photons
 *
 * Input:
 *   A binary photon stream
 *
 * Output:
 *   TIME       CHAN    OUTPUTS...
 *
 */

struct input_channel {
        int chan_n;
        uint64_t mask;

        input_channel(int chan_n, uint64_t mask) :
                chan_n(chan_n), mask(mask) { }
};

struct output_channel {
        int chan_n;
        uint64_t mask;
        bool state;

        output_channel(int chan_n, uint64_t mask) :
                chan_n(chan_n), mask(mask), state(false) { }
};

int main(int argc, char** argv) {
        std::vector<output_channel> outputs = {
                output_channel(0, CHAN_0_MASK),
                output_channel(1, CHAN_1_MASK),
                output_channel(2, CHAN_2_MASK),
                output_channel(3, CHAN_3_MASK)
        };
        std::vector<input_channel> inputs = {
                input_channel(0, CHAN_0_MASK),
                input_channel(1, CHAN_1_MASK),
                input_channel(2, CHAN_2_MASK),
                input_channel(3, CHAN_3_MASK)
        };

        setvbuf(stdout, NULL, _IONBF, NULL);
        while (true) {
                record_t photon = 0;

                if (fread(&photon, RECORD_LENGTH, 1, stdin) != 1)
                        exit(!feof(stdin));

                photon = be64toh(photon) >> 16;
                count_t time = photon & TIME_MASK;
                
                
                if (photon & REC_TYPE_MASK) {
                        // Delta record
                        for (auto chan=outputs.begin(); chan != outputs.end(); chan++)
                                chan->state = photon & chan->mask;
                } else  {
                        // Strobe record
                        for (auto chan=inputs.begin(); chan != inputs.end(); chan++) {
                                if (!(photon & chan->mask)) continue;
                                printf("%11llu\t%d\t", time, chan->chan_n);
                                for (auto out=outputs.begin(); out != outputs.end(); out++)
                                        printf("%d\t", out->state);
                                printf("\n");
                        }
                }
        }

        return 0;
}


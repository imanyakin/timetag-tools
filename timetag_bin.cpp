// vim: set fileencoding=utf-8 noet :

/* timetag-tools - Tools for UMass FPGA timetagger
 *
 * Copyright © 2010 Ben Gamari
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/ .
 *
 * Author: Ben Gamari <bgamari@physics.umass.edu>
 */


#include <vector>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include <boost/program_options.hpp>
#include "record.h"

namespace po = boost::program_options;

/*
 *
 * Temporally bins a photon stream
 *
 * Usage:
 *   bin_photons [--text] [BIN_LENGTH]
 *
 * Where BIN_LENGTH is the length of each bin in counter units.
 *
 * Input:
 *   A binary photon stream
 *
 * Output:
 *   A binary stream of bin_records or a textual representation if
 *   --text is given.
 *
 * Notes:
 *   We handle wrap-around here by simply keeping all times as 64-bit and
 *   assume that we wrap-around at most once.  With 1 nanosecond clock units,
 *   this gives us 500 years of acquisition time.
 *
 */

/*
 * bin record format
 */
struct bin_record {
        int chan_n;
        uint64_t start_time;
        unsigned int count;
        unsigned int lost;
};

struct input_channel {
        int chan_n;
        count_t bin_start;
        unsigned int count;
        unsigned int lost;      // IMPORTANT: This is not a count of lost photons, only
                                //            potential sprees of lost photons
        input_channel(int chan_n) :
                chan_n(chan_n), bin_start(0), count(0), lost(0) { }
};

void print_text_bin(bin_record b) {
        printf("%2d\t%10lu\t%5u\t%5u\n", b.chan_n, b.start_time, b.count, b.lost);
}

void print_bin(bin_record b) {
        if (write(1, &b, sizeof(bin_record)) < (int) sizeof(bin_record))
                throw new std::runtime_error("failed to write bin");
}

void handle_record(std::vector<input_channel>& chans, count_t bin_length, record& r,
                   std::function<void(bin_record)> print, bool with_zeros=true) {
        std::bitset<4> channels = r.get_channels();
        uint64_t time = r.get_time();
        for (auto c=chans.begin(); c != chans.end(); c++) {
                if (time >= (c->bin_start + bin_length)) {
                        uint64_t new_bin_start = (time / bin_length) * bin_length;
                        
                        // First print photons in last bin
                        struct bin_record rec = { c->chan_n, c->bin_start, c->count, c->lost };
                        if (with_zeros || c->count > 0)
                                print(rec);

                        // Then print zero bins
                        if (with_zeros) {
                                for (uint64_t t=c->bin_start+bin_length; t < new_bin_start; t += bin_length) {
                                        struct bin_record rec = { c->chan_n, t, 0, 0 };
                                        print(rec);
                                }
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
        count_t bin_length = 0;

        po::options_description desc("Allowed options");
        desc.add_options()
                ("help,h", "Display help message")
                ("bin-width",  po::value<count_t>(&bin_length)->required(), "The desired bin width")
                ("text,t", "Produce textual representation instead of usual binary output")
                ("omit-zeros,z", "Omit empty bins");

        po::positional_options_description pd;
        pd.add("bin-width", 1);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).positional(pd).run(), vm);
        po::notify(vm);

        if (vm.count("help")) {
                std::cout << desc << "\n";
                return 0;
        }

        bool text = vm.count("text");
        bool with_zeros = ! vm.count("omit-zeros");

        std::vector<input_channel> chans = {
                { input_channel(0) },
                { input_channel(1) },
                { input_channel(2) },
                { input_channel(3) },
        };
        record_stream stream(stdin);
        
        // Disable write buffering
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stdin, NULL, _IOFBF, sizeof(record)*30);

        /*
         * We throw away the first photon to get the bin start times.
         */
        {
                record r = stream.get_record();
                uint64_t time = r.get_time();
                for (auto c=chans.begin(); c != chans.end(); c++)
                        c->bin_start = (time / bin_length) * bin_length;
        }

        std::function<void(struct bin_record)> print = text ? print_text_bin : print_bin;
        while (true) {
                try {
                record r = stream.get_record();
                        handle_record(chans, bin_length, r, print, with_zeros);
                } catch (end_stream e) { break; }
        }

        return 0;
}


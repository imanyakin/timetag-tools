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
#include <iostream>
#include <boost/program_options.hpp>
#include "record.h"

namespace po = boost::program_options;

int main(int argc, char** argv) {
        po::options_description desc("Allowed options");
        desc.add_options()
                ("help,h", "Display help message")
                ("strobe-on,s", po::value<unsigned int>(), "include only records with strobe channel N active")
                ("delta-on,d", po::value<unsigned int>(), "include only records with delta channel N active")
                ("start-time,t", po::value<float>(), "start at timestamp TIME")
                ("end-time,T", po::value<float>(), "end at timestamp TIME")
                ("skip-records,r", po::value<unsigned int>(), "skip N records")
                ("truncate-records,R", po::value<unsigned int>(), "truncate all records past N")
                ("drop-initial-wraps,W", po::value<unsigned int>(), "ignore data until the Nth wrap-around")
                ("preserve-wraps,w", po::value<bool>(), "Keep wrap records");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        int strobe_on = -1, delta_on = -1;
        std::bitset<4> delta_status;
        uint64_t start_time = 0,  end_time = 1ULL << 63;
        unsigned int skip_records = 0, truncate_records = 0;
        unsigned int drop_wraps = 0;
        bool preserve_wraps = false;

        if (vm.count("help")) {
                std::cout << desc << "\n";
                return 0;
        }

        if (vm.count("strobe-on"))
                strobe_on = vm["strobe-on"].as<unsigned int>();

        if (vm.count("delta-on"))
                delta_on = vm["delta-on"].as<unsigned int>();

        if (vm.count("start-time"))
                start_time = round(vm["start-time"].as<float>());

        if (vm.count("end-time"))
                end_time = round(vm["end-time"].as<float>());

        if (vm.count("skip-records"))
                skip_records = vm["skip-records"].as<unsigned int>();

        if (vm.count("truncate-records"))
                truncate_records = vm["truncate-records"].as<unsigned int>();

        if (vm.count("drop-initial-wraps"))
                drop_wraps = vm["drop-initial-wraps"].as<unsigned int>();

        if (vm.count("preserve-wraps"))
                preserve_wraps = true;

        record_stream stream(stdin, drop_wraps);
        unsigned int i=0;
        while (true) {
                try {
                        bool drop = false;
                        record r = stream.get_record();
                        i++;

                        if (r.get_type() == record::DELTA) {
                                delta_status = r.get_channels();
                                continue;
                        }

                        // Temporal filters
                        if (r.get_time() > end_time) drop = true;
                        if (r.get_time() < start_time) drop = true;
                        if (i <= skip_records) drop = true;
                        if (truncate_records != 0 && i >= truncate_records) drop = true;

                        // Always keep wrap records but drop set channels
                        if (!drop && r.get_wrap_flag() && preserve_wraps) {
                                r.data &= ~CHANNEL_MASK;
                                write_record(stdout, r);
                        } else if (drop) {
                                continue;
                        } else {
                                // Channel filters
                                std::bitset<4> chans = r.get_channels();
                                if (strobe_on != -1 && !chans[strobe_on]) continue;
                                if (delta_on != -1 && !delta_status[delta_on]) continue;

                                write_record(stdout, r);
                        }
                } catch (end_stream e) { break; }
        }
}

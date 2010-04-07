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


#include <cstdint>
#include <stdexcept>
#include <bitset>
#include <vector>
#include <array>
#include "record_format.h"

struct end_stream : std::exception { };

struct record {
        record_t data;
        uint64_t time_offset;
        enum type { STROBE, DELTA };

        record(record_t data, int64_t time_offset=0) : data(data), time_offset(time_offset) { }
        type get_type() const;
        uint64_t get_time() const;
        uint64_t get_raw_time() const;
        bool get_wrap_flag() const;
        bool get_lost_flag() const;
        std::bitset<4> get_channels() const;
};

struct parsed_record {
        uint64_t time;
        record::type type;
        bool wrap;
        bool lost;
        std::array<bool,4> channels;
};

class record_stream {
        uint64_t time_offset;
        int fd;
       
public:
        record_stream(int fd);
        record get_record();
        std::vector<parsed_record> parse_records(unsigned int n);
};

unsigned int get_file_length(const char* path);
void write_record(int fd, record r);


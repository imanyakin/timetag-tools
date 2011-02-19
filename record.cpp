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


#include "record.h"
#include <sys/stat.h>

record::type record::get_type() const {
        return (data & REC_TYPE_MASK) ? DELTA : STROBE;
}

uint64_t record::get_time() const {
        return get_raw_time() + time_offset;
}

uint64_t record::get_raw_time() const {
        return data & TIME_MASK;
}

bool record::get_wrap_flag() const {
        return (data & TIMER_WRAP_MASK) != 0;
}

bool record::get_lost_flag() const {
        return (data & LOST_SAMPLE_MASK) != 0;
}

std::bitset<4> record::get_channels() const {
        return std::bitset<4>((data & CHANNEL_MASK) >> TIME_BITS);
}

record_stream::record_stream(int fd) : time_offset(0), fd(fd) { }

unsigned int get_file_length(const char* path) {
        struct stat buf;
        int res;

        res = stat(path, &buf);
        if (res)
                throw std::runtime_error("Error in stat()");
        
        return buf.st_size / RECORD_LENGTH;
}

record record_stream::get_record() {
        record_t data;
        int res = read(fd, &data, RECORD_LENGTH);
        if (res == 0)
                throw end_stream();
        else if (res < RECORD_LENGTH)
                throw std::runtime_error("Incomplete record");

#if defined(LITTLE_ENDIAN)
        uint8_t* d = (uint8_t*) &data;
        std::swap(d[0], d[5]);
        std::swap(d[1], d[4]);
        std::swap(d[2], d[3]);
#elif defined(BIG_ENDIAN)
#else
#error Either LITTLE_ENDIAN or BIG_ENDIAN must be defined.
#endif
        
        record rec(data);
        if (rec.get_wrap_flag())
                time_offset += (1ULL<<TIME_BITS) - 1;
        rec.time_offset = time_offset;
        return rec;
}

std::vector<parsed_record> record_stream::parse_records(unsigned int n) {
        std::vector<parsed_record> buf;

        for (unsigned int i=0; i<n; i++) {
                record r = get_record();
                parsed_record pr;

                pr.time = r.get_time();
                pr.type = r.get_type();
                pr.wrap = r.get_wrap_flag();
                pr.lost = r.get_lost_flag();

                std::bitset<4> ch = r.get_channels();
                for (unsigned int j=0; j<4; j++) 
                        pr.channels[j] = ch[j];

                buf.push_back(pr);
        }
        return buf;
}

void write_record(int fd, record r) {
        record_t data;
        data = r.data;
        data = htobe64(data << 16);

        int res = write(fd, &data, RECORD_LENGTH);
        if (res == 0)
                throw end_stream();
        else if (res < RECORD_LENGTH)
                throw std::runtime_error("Incomplete record");
}


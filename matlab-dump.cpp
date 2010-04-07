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
#include <iostream>
#include <vector>
#include <cstdint>

using std::vector;
using std::cout;
using std::string;

enum matlab_type {
        INT8    = 1,
        UINT8   = 2,
        INT16   = 3,
        UINT16  = 4,
        INT32   = 5,
        UINT32  = 6,
        SINGLE  = 7,
        DOUBLE  = 9,
        INT64   = 12,
        UINT64  = 13,
        MATRIX  = 14,
};

void write_header(string description) {
        uint16_t version = 0x0100;
        uint16_t endian = 0x4d6c; // 'Ml'

        description.resize(124);
        cout.write(&description[0], 124);
        cout.write((char*) &version, 2);
        cout.write((char*) &endian, 2);
}

void write_tag(matlab_type type, uint32_t bytes) {
        uint32_t t = type;
        cout.write((char*) &t, 4);
        cout.write((char*) &bytes, 4);
}

int main(int argc, char** argv) {
        record_stream stream(0);
        vector<record> records;
        
        while (true) {
                try {
                        records.push_back(stream.get_record());
                } catch (end_stream e) { break; }
        }

        write_header("timetag matlab-dump");
        write_tag(MATRIX, records.size() * (8 + 1 + 1 + 4*1));

        // Write time
        write_tag(UINT64, records.size() * 8);
        for (auto r=records.begin(); r != records.end(); r++) {
                uint64_t time = r->get_time();
                cout.write((char*) &time, 8);
        }

#define WRITE_FLAG(ACCESSOR) \
        write_tag(UINT8, records.size()); \
        for (auto r=records.begin(); r != records.end(); r++) { \
                char flag = r-> ACCESSOR; \
                cout.write((char*) &flag, 1); \
        }

        WRITE_FLAG(get_wrap_flag());
        WRITE_FLAG(get_lost_flag());
        
        WRITE_FLAG(get_channels()[0]);
        WRITE_FLAG(get_channels()[1]);
        WRITE_FLAG(get_channels()[2]);
        WRITE_FLAG(get_channels()[3]);

        return 0;
}


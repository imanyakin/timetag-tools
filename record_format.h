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


#ifndef _PHOTON_FORMAT_H
#define _PHOTON_FORMAT_H

#include <cstdint>

typedef uint64_t record_t;
typedef uint64_t count_t; // Represents a time counter value


#define RECORD_LENGTH 6
#define TIME_BITS 36
#define TIME_MASK ((1ULL<<TIME_BITS) - 1)

#define CHAN_0_MASK (1ULL << 36)
#define CHAN_1_MASK (1ULL << 37)
#define CHAN_2_MASK (1ULL << 38)
#define CHAN_3_MASK (1ULL << 39)
#define CHANNEL_MASK (CHAN_0_MASK | CHAN_1_MASK | CHAN_2_MASK | CHAN_3_MASK)

#define REC_TYPE_MASK (1ULL << 45)
#define TIMER_WRAP_MASK (1ULL << 46)
#define LOST_SAMPLE_MASK (1ULL << 47)

#endif


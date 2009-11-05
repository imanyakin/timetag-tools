#ifndef _PHOTON_FORMAT_H
#define _PHOTON_FORMAT_H

typedef unsigned long long record_t;
typedef unsigned long long count_t; // Represents a time counter value

#define RECORD_LENGTH 6
#define TIME_BITS 39
#define TIME_MASK ((1ULL<<TIME_BITS) - 1)

#define CHAN_1_MASK (1ULL << 41)
#define CHAN_2_MASK (1ULL << 42)
#define CHAN_3_MASK (1ULL << 43)
#define CHAN_4_MASK (1ULL << 44)
#define CHANNEL_MASK (CHAN_1_MASK | CHAN_2_MASK | CHAN_3_MASK | CHAN_4_MASK)

#endif


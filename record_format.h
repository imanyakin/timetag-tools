#ifndef _PHOTON_FORMAT_H
#define _PHOTON_FORMAT_H

typedef unsigned long long record_t;
typedef unsigned long long count_t; // Represents a time counter value

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


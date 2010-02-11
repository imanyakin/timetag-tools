#include "record.h"

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


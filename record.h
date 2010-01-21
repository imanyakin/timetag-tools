#include <cstdint>
#include <bitset>
#include "record_format.h"


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

class record_stream {
        uint64_t time_offset;
        int fd;
       
public:
        record_stream(int fd);
        record get_record();
};


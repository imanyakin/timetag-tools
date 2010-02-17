#include <vector>
#include <iostream>
#include "record.h"

int main(int argc, char** argv) {
	record_stream stream(0);
	while (true) {
		try {
			record r = stream.get_record();
			uint64_t time = r.get_time();
			write(1, &time, sizeof(uint64_t));
		} catch (end_stream e) { break; }
	}
}


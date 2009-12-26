#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <libusb.h>

#include "timetagger.h"
#include "photon_format.h"

#define VENDOR_ID 0x04b4
#define PRODUCT_ID 0x1004


struct data_cb : timetagger::data_cb_t {
	void operator()(const uint8_t* buffer, size_t length) {
		std::cout.write((const char*) buffer, length);
	}
};

static void read_loop(timetagger& t)
{
	int ret, transferred;
	t.start_readout();

	// Command loop
	while (1) {
		std::string cmd;
		std::getline(std::cin, cmd, ' ');

		if (cmd == "start_cap") {
			t.start_capture();
		}
		if (cmd == "stop_cap") {
			t.stop_capture();
		}
		if (cmd == "reset") {
			t.reset_counter();
		}
		if (cmd == "initial_state") {
			int mask, state;
			std::cin >> mask;
			std::cin >> state;
			t.pulseseq_set_initial_state(mask, state != 0);
		}
		if (cmd == "initial_count") {
			int mask, count;
			std::cin >> mask;
			std::cin >> count;
			t.pulseseq_set_initial_count(mask, count);
		}
		if (cmd == "high_count") {
			int mask, count;
			std::cin >> mask;
			std::cin >> count;
			t.pulseseq_set_high_count(mask, count);
		}
		if (cmd == "low_count") {
			int mask, count;
			std::cin >> mask;
			std::cin >> count;
			t.pulseseq_set_low_count(mask, count);
		}
	}

	t.stop_readout();
}

int main(int argc, char** argv)
{
	libusb_context* ctx;
	libusb_device_handle* dev;
       
	libusb_init(&ctx);
	dev = libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID);
	if (!dev) {
		fprintf(stderr, "Failed to open device.\n");
		exit(1);
	}

	data_cb cb;
	timetagger t(dev, cb);

	t.get_status();
	t.stop_capture();

#if 0
	t.pulseseq_set_initial_state(0x04, true);
	t.pulseseq_set_initial_count(0x04, 10000);
	t.pulseseq_set_low_count(0x04, 20000);
        t.pulseseq_set_high_count(0x04, 30000);
#endif

	read_loop(t);

	libusb_close(dev);
}


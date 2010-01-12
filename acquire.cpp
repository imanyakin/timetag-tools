#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <libusb.h>

#include "timetagger.h"
#include "photon_format.h"

#define VENDOR_ID 0x04b4
#define PRODUCT_ID 0x1004


struct data_cb : timetagger::data_cb_t {
	void operator()(const uint8_t* buffer, size_t length) {
		fwrite((const char*) buffer, 1, length, stdout);
	}
};

static void read_loop(timetagger& t)
{
	int ret, transferred;
	t.start_readout();

	// Command loop
	while (!std::cin.eof()) {
		std::string cmd, tmp;
		std::getline(std::cin, cmd);

		if (cmd == "start_capture") {
			t.start_capture();
		} else if (cmd == "stop_capture") {
			t.stop_capture();
		} else if (cmd == "reset") {
			t.reset_counter();
		} else if (cmd == "set_initial_state") {
			int output, state;
			std::cin >> output;
			std::cin >> state;
			t.pulseseq_set_initial_state(output, state != 0);
		} else if (cmd == "set_initial_count") {
			int output, count;
			std::cin >> output;
			std::cin >> count;
			t.pulseseq_set_initial_count(output, count);
		} else if (cmd == "set_high_count") {
			int output, count;
			std::cin >> output;
			std::cin >> count;
			t.pulseseq_set_high_count(output, count);
		} else if (cmd == "set_low_count") {
			int output, count;
			std::cin >> output;
			std::cin >> count;
			t.pulseseq_set_low_count(output, count);
		} else
			std::cerr << "Invalid command\n";
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
	read_loop(t);

	libusb_close(dev);
}


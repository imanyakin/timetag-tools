#include <cstdio>
#include <cstring>
#include "timetagger.h"
#include "photon_format.h"

#define CMD_ENDP	0x02
#define DATA_ENDP	0x86
#define REPLY_ENDP	0x88

#define TIMEOUT 100
#define DATA_TIMEOUT 5000

#define TRANSFER_LEN (85*RECORD_LENGTH)

void timetagger::send_simple_command(uint8_t mask, cmd_data data)
{
	std::vector<uint8_t> buffer = {
		0xAA,			// Magic number
		(uint8_t) data.size(),	// Command length
		mask			// Command mask
	};
	for (int i=0; i<data.size(); i++)
		buffer.push_back(data[i]);

	int ret, transferred;
	if (ret = libusb_bulk_transfer(dev, CMD_ENDP, &buffer[0], buffer.size(), &transferred, TIMEOUT) )
		fprintf(stderr, "Failed sending request: %d\n", ret);

#if 0
	if (ret = libusb_bulk_transfer(dev, REPLY_ENDP, buffer, 128, &transferred, TIMEOUT) )
		fprintf(stderr, "failed receiving reply: %d\n", ret);

	if (transferred > 4)
		fprintf(stderr, "Length: %d\n", ((int*) buffer)[0]);
	else
		fprintf(stderr, "No response\n");
#endif
}

void timetagger::pulseseq_set_initial_state(char seq_mask, bool state)
{
	cmd_data data = { 0x00, 0x00, 0x00, (uint8_t) (state ? 0xff : 0x00), 0x01 };
	send_simple_command(seq_mask, data);
}

void timetagger::pulseseq_set_initial_count(char seq_mask, uint32_t count)
{
	cmd_data data = {
	       	(uint8_t) (0xff & (count >> 24)),
		(uint8_t) (0xff & (count >> 16)),
		(uint8_t) (0xff & (count >> 8)),
		(uint8_t) (0xff & (count >> 0)),
		0x02 };
	send_simple_command(seq_mask, data);
}

void timetagger::pulseseq_set_high_count(char seq_mask, uint32_t count)
{
	cmd_data data = {
		(uint8_t) (0xff & (count >> 24)),
		(uint8_t) (0xff & (count >> 16)),
		(uint8_t) (0xff & (count >> 8)),
		(uint8_t) (0xff & (count >> 0)),
		0x04 };
	send_simple_command(seq_mask, data);
}

void timetagger::pulseseq_set_low_count(char seq_mask, uint32_t count)
{
	cmd_data data = {
		(uint8_t) (0xff & (count >> 24)),
		(uint8_t) (0xff & (count >> 16)),
		(uint8_t) (0xff & (count >> 8)),
		(uint8_t) (0xff & (count >> 0)),
		0x08 };
	send_simple_command(seq_mask, data);
}

void timetagger::pulseseq_start()
{
	cmd_data data = { 0x1 };
	send_simple_command(0x02, data);
}

void timetagger::pulseseq_stop()
{
	cmd_data data = { 0x2 };
	send_simple_command(0x02, data);
}

void timetagger::start_capture()
{
	cmd_data data = { 0x1 };
	send_simple_command(0x01, data);
}

void timetagger::stop_capture()
{
	cmd_data data = { 0x2 };
	send_simple_command(0x01, data);
}

void timetagger::reset_counter()
{
	cmd_data data = { 0x4 };
	send_simple_command(0x01, data);
}

struct readout_handler {
	libusb_device_handle* dev;
	timetagger::data_cb_t& cb;

	readout_handler(libusb_device_handle* dev, timetagger::data_cb_t& cb) :
		dev(dev), cb(cb) { }

	void operator()()
	{
		uint8_t* buffer = new uint8_t[TRANSFER_LEN];

		do {
			int transferred, res;
			res = libusb_bulk_transfer(dev, DATA_ENDP, buffer, TRANSFER_LEN, &transferred, 0);
			if (!res) {
				if (transferred % RECORD_LENGTH != 0)
					fprintf(stderr, "Warning: Received partial record.");
				cb(buffer, transferred);
			} else
				fprintf(stderr, "Transfer failed: %d\n", res);

			try {
				boost::this_thread::interruption_point();
			} catch (...) {
				break;
			}
		} while (true);
		delete [] buffer;
	}
};

void timetagger::start_readout() 
{
	assert(readout_thread == 0);
	readout_handler h(dev, data_cb);
	readout_thread = boost::shared_ptr<boost::thread>(new boost::thread(h));
}

void timetagger::stop_readout() {
	assert(readout_thread != 0);
	readout_thread->interrupt();
	readout_thread->join();
}


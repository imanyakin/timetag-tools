#include <cstdio>
#include <cstring>
#include "timetagger.h"
#include "record_format.h"

#define CMD_ENDP	0x02
#define DATA_ENDP	0x86
#define REPLY_ENDP	0x88

#define TIMEOUT 100
#define DATA_TIMEOUT 5000

#define TRANSFER_LEN (85*RECORD_LENGTH)

#define REQ_TYPE_VENDOR 0x02


void timetagger::reset()
{
	stop_capture();
	flush_fx2_fifo();
	needs_flush = true;
}

void timetagger::send_simple_command(uint8_t mask, cmd_data data)
{
	std::vector<uint8_t> buffer = {
		0xAA,			// Magic number
		(uint8_t) data.size(),	// Command length
		mask			// Command mask
	};
	for (unsigned int i=0; i<data.size(); i++)
		buffer.push_back(data[i]);

	int ret, transferred;
	if ( (ret = libusb_bulk_transfer(dev, CMD_ENDP, &buffer[0], buffer.size(), &transferred, TIMEOUT)) )
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

void timetagger::pulseseq_start(std::tr1::array<bool, 4> units)
{
	uint8_t d = 
		(units[0] ? 0x01 : 0x00) |
		(units[1] ? 0x04 : 0x00) |
		(units[2] ? 0x10 : 0x00) |
		(units[3] ? 0x40 : 0x00);
	cmd_data data = { d };
	send_simple_command(0x02, data);
}

void timetagger::pulseseq_stop(std::tr1::array<bool, 4> units)
{
	uint8_t d = 
		(units[0] ? 0x02 : 0x00) |
		(units[1] ? 0x08 : 0x00) |
		(units[2] ? 0x20 : 0x00) |
		(units[3] ? 0x80 : 0x00);
	/*
	 * We include an extra byte as an _evil_ hack to work around
	 * unidentified bug in command parser
	 */
	cmd_data data = { d, 0x00 };
	send_simple_command(0x02, data);
}

void timetagger::start_capture()
{
	// Don't start capture until flush has finished
	while (needs_flush)
		sleep(1);
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

void timetagger::set_send_window(unsigned int records)
{
	unsigned int bytes = RECORD_LENGTH*records;
	if (bytes > 512) {
		fprintf(stderr, "Error: Send window too large\n");
		return;
	}

	uint8_t data[2] = {
		(uint8_t) (bytes & 0xff),
		(uint8_t) ((bytes >> 8) & 0x8)
	};
	int res = libusb_control_transfer(dev, REQ_TYPE_VENDOR, 0x01, 0, 0, data, 2, 0);
	if (!res)
		fprintf(stderr, "Error setting send window size: %d\n", res);
}

void timetagger::flush_fx2_fifo() {
	// Request FIFO flush
	int res = libusb_control_transfer(dev, REQ_TYPE_VENDOR, 0x02, 0, 0, NULL, 0, 0);
	if (!res)
		fprintf(stderr, "Error requesting FX2 FIFO flush: %d\n", res);
	usleep(1000);
}

void timetagger::readout_handler::do_flush() {
	int transferred = 0;
	do {
		uint8_t buffer[512];
		libusb_bulk_transfer(dev, DATA_ENDP, buffer, 512, &transferred, 10);
	} while (transferred > 0);
	needs_flush = false;
}

void timetagger::readout_handler::operator()()
{
	uint8_t* buffer = new uint8_t[TRANSFER_LEN];

	do {
		int transferred, res;
		res = libusb_bulk_transfer(dev, DATA_ENDP, buffer, TRANSFER_LEN, &transferred, data_timeout);
		if (!res) {
			if (transferred % RECORD_LENGTH != 0)
				fprintf(stderr, "Warning: Received partial record.");
			cb(buffer, transferred);
		} else if (res == LIBUSB_ERROR_TIMEOUT) {
			// Ignore timeouts, we just need to fall through to the
			// needs_flush check every once in a while
		} else
			fprintf(stderr, "Transfer failed: %d\n", res);

		if (needs_flush)
			do_flush();

		try {
			boost::this_thread::interruption_point();
		} catch (...) {
			break;
		}
	} while (true);
	delete [] buffer;
}

void timetagger::start_readout() 
{
	assert(readout_thread == 0);
	readout_handler readout(dev, data_cb, needs_flush);
	readout_thread = boost::shared_ptr<boost::thread>(new boost::thread(readout));
}

void timetagger::stop_readout() {
	assert(readout_thread != 0);
	readout_thread->interrupt();
	readout_thread->join();
}


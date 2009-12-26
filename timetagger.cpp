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

void timetagger::send_simple_command(uint8_t mask, uint8_t* data, size_t length)
{
	int ret, transferred;
	uint8_t* buffer = new uint8_t[1024];
	memset(buffer, 0, 1024);

	buffer[0] = length;
	buffer[1] = mask;
	for (int i=0; i<length; i++)
		buffer[i+2] = data[i];

	if (ret = libusb_bulk_transfer(dev, CMD_ENDP, buffer, 128, &transferred, TIMEOUT) )
		fprintf(stderr, "Failed sending request: %d\n", ret);
	if (ret = libusb_bulk_transfer(dev, REPLY_ENDP, buffer, 128, &transferred, TIMEOUT) )
		fprintf(stderr, "failed receiving reply: %d\n", ret);

	if (transferred > 4)
		fprintf(stderr, "Length: %d\n", ((int*) buffer)[0]);
	else
		fprintf(stderr, "No response\n");

	delete [] buffer;
}

void timetagger::pulseseq_set_initial_state(char seq_mask, bool state)
{
	uint8_t buffer[5] = { 0x00, 0x00, 0x00, state, 0x01 };
	send_simple_command(seq_mask, buffer, 5);
}

void timetagger::pulseseq_set_initial_count(char seq_mask, uint32_t count)
{
	uint8_t buffer[5] = {
		0xff & (count >> 24),
		0xff & (count >> 16),
		0xff & (count >> 8),
	       	0xff & (count >> 0),
		0x02 };
	send_simple_command(seq_mask, buffer, 5);
}

void timetagger::pulseseq_set_high_count(char seq_mask, uint32_t count)
{
	uint8_t buffer[5] = {
		0xff & (count >> 24),
		0xff & (count >> 16),
		0xff & (count >> 8),
		0xff & (count >> 0),
		0x04 };
	send_simple_command(seq_mask, buffer, 5);
}

void timetagger::pulseseq_set_low_count(char seq_mask, uint32_t count)
{
	uint8_t buffer[5] = {
		0xff & (count >> 24),
		0xff & (count >> 16),
		0xff & (count >> 8),
		0xff & (count >> 0),
		0x04 };
	send_simple_command(seq_mask, buffer, 5);
}

void timetagger::pulseseq_start()
{
	uint8_t buffer[1] = { 0x1 };
	send_simple_command(0x02, buffer, 1);
}

void timetagger::pulseseq_stop()
{
	uint8_t buffer[1] = { 0x2 };
	send_simple_command(0x02, buffer, 1);
}

void timetagger::start_capture()
{
	uint8_t buffer[1] = { 0x1 };
	send_simple_command(0x01, buffer, 1);
}

void timetagger::stop_capture()
{
	uint8_t buffer[1] = { 0x2 };
	send_simple_command(0x01, buffer, 1);
}

void timetagger::reset_counter()
{
	uint8_t buffer[1] = { 0x4 };
	send_simple_command(0x01, buffer, 1);
}

void timetagger::get_status()
{
	int ret, transferred;
	uint8_t buffer[1024];

	if (ret = libusb_bulk_transfer(dev, 0x81, &buffer[0], 1024, &transferred, TIMEOUT) )
		fprintf(stderr, "Failed sending request: %d\n", ret);
	fprintf(stderr, "Transferred: %d:\n", transferred);
        for (int i=0; i<transferred; i++)
		fprintf(stderr, "%02x ", buffer[i]);
	fprintf(stderr, "\n");
}


static void transfer_done_cb(libusb_transfer* transfer)
{
	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) 
		fprintf(stderr, "Failed sending request: %d\n", transfer->status);

	if (transfer->actual_length % RECORD_LENGTH != 0)
		fprintf(stderr, "Warning: Received partial record.");

	timetagger* me = (timetagger*) transfer->user_data;
	me->data_cb(transfer->buffer, transfer->actual_length);

	libusb_submit_transfer(transfer);
}

void timetagger::start_readout() 
{
	data_buffer = new uint8_t[TRANSFER_LEN];
	transfer = libusb_alloc_transfer(0);
	libusb_fill_bulk_transfer(transfer, dev, DATA_ENDP, data_buffer, TRANSFER_LEN, transfer_done_cb, this, 0);
	libusb_submit_transfer(transfer);
}

void timetagger::stop_readout() {
	libusb_cancel_transfer(transfer);
	libusb_free_transfer(transfer);
	delete [] data_buffer;
}


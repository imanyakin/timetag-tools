#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <libusb.h>

#include "photon_format.h"

#define VENDOR_ID 0x04b4
#define PRODUCT_ID 0x1004

#define CMD_ENDP	0x02
#define DATA_ENDP	0x86
#define REPLY_ENDP	0x88

struct buffer {
	struct libusb_transfer* transfer;
	struct buffer* next;
	int size;
	uint8_t data[0];
};

#define TIMEOUT 100
#define DATA_TIMEOUT 5000

#define TRANSFER_LEN (85*RECORD_LENGTH)

struct buffer* buffers;

static void send_simple_command(libusb_device_handle* dev, uint8_t mask, uint8_t* data, int len) {
	int ret, transferred;
	uint8_t* buffer = (uint8_t*) calloc(sizeof(uint8_t), 1024);

	buffer[0] = len;
	buffer[1] = mask;
	for (int i=0; i<len; i++)
		buffer[i+2] = data[i];

	if (ret = libusb_bulk_transfer(dev, CMD_ENDP, buffer, 128, &transferred, TIMEOUT) )
		fprintf(stderr, "Failed sending request: %d\n", ret);
	if (ret = libusb_bulk_transfer(dev, REPLY_ENDP, buffer, 128, &transferred, TIMEOUT) )
		fprintf(stderr, "failed receiving reply: %d\n", ret);

	if (transferred > 4)
		fprintf(stderr, "Length: %d\n", ((int*) buffer)[0]);
	else
		fprintf(stderr, "No response\n");
}

void pulseseq_set_initial_state(libusb_device_handle* dev, char seq_mask, bool initial_state) {
	uint8_t buffer[5] = { 0x00, 0x00, 0x00, initial_state, 0x01 };
	send_simple_command(dev, 0x4, buffer, 5);
}

void pulseseq_set_initial_count(libusb_device_handle* dev, char seq_mask, uint32_t initial_count) {
	uint8_t buffer[5] = {
		0xff & (initial_count >> 24),
		0xff & (initial_count >> 16),
		0xff & (initial_count >> 8),
	       	0xff & (initial_count >> 0),
		0x02 };
	send_simple_command(dev, 0x4, buffer, 5);
}

void pulseseq_set_high_count(libusb_device_handle* dev, char seq_mask, uint32_t high_count) {
	uint8_t buffer[5] = {
		0xff & (high_count >> 24),
		0xff & (high_count >> 16),
		0xff & (high_count >> 8),
		0xff & (high_count >> 0),
		0x04 };
	send_simple_command(dev, 0x4, buffer, 5);
}

void pulseseq_set_low_count(libusb_device_handle* dev, char seq_mask, uint32_t low_count) {
	uint8_t buffer[5] = {
		0xff & (low_count >> 24),
		0xff & (low_count >> 16),
		0xff & (low_count >> 8),
		0xff & (low_count >> 0),
		0x04 };
	send_simple_command(dev, 0x4, buffer, 5);
}

void start_capture(libusb_device_handle* dev) {
	uint8_t buffer[1] = { 0x1 };
	send_simple_command(dev, 0x1, buffer, 1);
}

void stop_capture(libusb_device_handle* dev) {
	uint8_t buffer[1] = { 0x2 };
	send_simple_command(dev, 0x1, buffer, 1);
}

void reset_counter(libusb_device_handle* dev) {
	uint8_t buffer[1] = { 0x4 };
	send_simple_command(dev, 0x1, buffer, 1);
}

void get_status(libusb_device_handle* dev) {
	int ret, transferred;
	uint8_t buffer[1024];

	if (ret = libusb_bulk_transfer(dev, 0x81, &buffer[0], 1024, &transferred, TIMEOUT) )
		fprintf(stderr, "Failed sending request: %d\n", ret);
	fprintf(stderr, "Transferred: %d:\n", transferred);
        for (int i=0; i<transferred; i++)
		fprintf(stderr, "%02x ", buffer[i]);
	fprintf(stderr, "\n");
}


static void transfer_done_cb(libusb_transfer* transfer) {
	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) 
		fprintf(stderr, "Failed sending request: %d\n", transfer->status);

	if (transfer->actual_length % RECORD_LENGTH != 0)
		fprintf(stderr, "Warning: Received partial record.");

	fwrite(transfer->buffer, 1, transfer->actual_length, stdout);
	libusb_submit_transfer(transfer);
}

static void read_loop(libusb_device_handle* dev) {
	int ret, transferred;
	uint8_t *buffer = (uint8_t*) calloc(TRANSFER_LEN, 1);

	libusb_transfer* transfer = libusb_alloc_transfer(0);
	libusb_fill_bulk_transfer(transfer, dev, 0x86, buffer, TRANSFER_LEN, transfer_done_cb, NULL, 0);
	libusb_submit_transfer(transfer);

	// Command loop
	while (1) {
	}
	
	libusb_cancel_transfer(transfer);
	libusb_free_transfer(transfer);
}

int main(int argc, char** argv) {
	libusb_context* ctx;
	libusb_device_handle* dev;
       
	libusb_init(&ctx);
	dev = libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID);
	if (!dev) {
		fprintf(stderr, "Failed to open device.\n");
		exit(1);
	}
	libusb_claim_interface(dev, 0);


	get_status(dev);

	start_capture(dev);

	pulseseq_set_initial_state(dev, 0x70, true);
	pulseseq_set_initial_count(dev, 0x70, 10000);
	pulseseq_set_low_count(dev, 0x70, 20000);
        pulseseq_set_high_count(dev, 0x70, 30000);

	read_loop(dev);

	libusb_release_interface(dev, 0);
	libusb_close(dev);
}


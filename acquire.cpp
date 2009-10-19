#include <cstdlib>
#include <cstdio>
#include <libusb.h>

#define VENDOR_ID 0x04b4
#define PRODUCT_ID 0x8613

#define CMD_ENDP 2
#define DATA_ENDP 6
#define REPLY_ENDP 8

struct buffer {
	struct libusb_transfer* transfer;
	struct buffer* next;
	int size;
	uint8_t data[0];
};

struct buffer* buffers;

void configure_pulse_sequencer(libusb_device_handle* dev, char seq_mask, bool initial_state, int initial_count, int high_count, int low_count) {
	int ret, transferred;
	uint8_t* buffer = (uint8_t*) calloc(sizeof(uint8_t), 128);

	buffer[0] = seq_mask;
	buffer[1] = initial_state;
	int* tmp = (int*) &buffer[2];
	tmp[0] = initial_count;
	tmp[1] = high_count;
	tmp[2] = low_count;

	if (!(ret = libusb_bulk_transfer(dev, CMD_ENDP, buffer, 128, &transferred, 0) ))
		printf("Failed sending pulse sequencer configuration request: %d\n", ret);
	if (!(ret = libusb_bulk_transfer(dev, REPLY_ENDP, buffer, 128, &transferred, 0) ))
		printf("Failed receiving reply: %d\n", ret);

	if (transferred > 4)
		printf("Length: %d\n", ((int*) buffer)[0]);
}

static void send_simple_command(libusb_device_handle* dev, char cmd) {
	int ret, transferred;
	uint8_t* buffer = (uint8_t*) calloc(sizeof(uint8_t), 1024);
	buffer[0] = cmd;
	if (!(ret = libusb_bulk_transfer(dev, CMD_ENDP, buffer, 128, &transferred, 0) ))
		printf("Failed sending request: %d\n", ret);
	if (!(ret = libusb_bulk_transfer(dev, REPLY_ENDP, buffer, 128, &transferred, 0) ))
		printf("failed receiving reply: %d\n", ret);

	if (transferred > 4)
		printf("Length: %d\n", ((int*) buffer)[0]);
}

void start_capture(libusb_device_handle* dev) {
	send_simple_command(dev, 0x4);
}

void stop_capture(libusb_device_handle* dev) {
	send_simple_command(dev, 0x2);
}

void reset_counter(libusb_device_handle* dev) {
	send_simple_command(dev, 0x1);
}


static void data_ready_cb(struct libusb_transfer* transfer) {
	struct buffer* buf = (struct buffer*) transfer->user_data;
	for (int i=0; i<transfer->actual_length; i++)
		printf("%c ", transfer->buffer[i]);
	libusb_free_transfer(transfer);
	free(buf);
}

void read_data(libusb_device_handle* dev) {
	const int buf_sz = 1024;

	struct buffer* buf = (struct buffer*) malloc(sizeof(struct buffer) + buf_sz);
	buf->size = buf_sz;
	buf->transfer = libusb_alloc_transfer(0);
	libusb_fill_bulk_transfer(buf->transfer, dev, DATA_ENDP, &buf->data[0], buf_sz, data_ready_cb, buf, 0);
	libusb_submit_transfer(buf->transfer);
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

	//configure_pulse_sequencer(dev, 0x70, true, 10000, 20000, 30000);
	//start_capture(dev);

	read_data(dev);
	while (1) { }

	libusb_release_interface(dev, 0);
	libusb_close(dev);
}


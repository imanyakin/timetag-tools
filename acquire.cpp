#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <libusb.h>

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

	if (ret = libusb_bulk_transfer(dev, CMD_ENDP, buffer, 128, &transferred, TIMEOUT) )
		fprintf(stderr, "Failed sending pulse sequencer configuration request: %d\n", ret);
	if (ret = libusb_bulk_transfer(dev, REPLY_ENDP, buffer, 128, &transferred, TIMEOUT) )
		fprintf(stderr, "Failed receiving reply: %d\n", ret);

	if (transferred >= 2)
		fprintf(stderr, "Length: %hu\n", ((uint16_t*) buffer)[0]);
}

static void send_simple_command(libusb_device_handle* dev, char cmd) {
	int ret, transferred;
	uint8_t* buffer = (uint8_t*) calloc(sizeof(uint8_t), 1024);
	buffer[0] = cmd;
	if (ret = libusb_bulk_transfer(dev, CMD_ENDP, buffer, 128, &transferred, TIMEOUT) )
		fprintf(stderr, "Failed sending request: %d\n", ret);
	if (ret = libusb_bulk_transfer(dev, REPLY_ENDP, buffer, 128, &transferred, TIMEOUT) )
		fprintf(stderr, "failed receiving reply: %d\n", ret);

	if (transferred > 4)
		fprintf(stderr, "Length: %d\n", ((int*) buffer)[0]);
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

void get_status(libusb_device_handle* dev) {
	int ret, transferred;
	uint8_t buffer[1024];

	if (ret = libusb_bulk_transfer(dev, 0x81, &buffer[0], 1024, &transferred, TIMEOUT) )
		fprintf(stderr, "Failed sending request: %d\n", ret);
	fprintf(stderr, "Transferred: %d:\n", transferred);
        for (int i=0; i<transferred; i++)
		printf("%02x ", buffer[i]);
	printf("\n");
}

void test2(libusb_device_handle* dev) {
	int ret, transferred;
	const int N = 85;
	uint8_t buffer[512];

	if (ret = libusb_bulk_transfer(dev, 0x86, &buffer[0], 512, &transferred, TIMEOUT) )
		fprintf(stderr, "Failed sending request: %d\n", ret);
#if 0
	printf("Transferred: %d:\n", transferred);
        for (int i=0; i<transferred; i++)
		printf("%02x ", buffer[i]);
	printf("\n");
#else
	for (int i=0; i<transferred/6; i++) {
		uint64_t a = *((uint64_t*) &buffer[i*6]);
		printf("  t=%lld\t%d  %d  %d  %d\n",
				a & 0x0FffffFFFFL,
				!!(a & (1L << 44)),
				!!(a & (1L << 45)),
				!!(a & (1L << 46)),
				!!(a & (1L << 47)));
	}
#endif
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
	stop_capture(dev);
	//for (int i=0; i<3; i++)
		//test2(dev);
	start_capture(dev);
	for (int i=0; i<3; i++)
		test2(dev);
	//stop_capture(dev);
	//for (int i=0; i<3; i++)
		//test2(dev);
	configure_pulse_sequencer(dev, 0x70, true, 10000, 20000, 30000);
	fprintf(stderr, "Done\n");

	//read_data(dev);

	libusb_release_interface(dev, 0);
	libusb_close(dev);
}


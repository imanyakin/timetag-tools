/* timetag-tools - Tools for UMass FPGA timetagger
 *
 * Copyright © 2010 Ben Gamari
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/ .
 *
 * Author: Ben Gamari <bgamari@physics.umass.edu>
 */


#include <cstdio>
#include <cstring>
#include "timetagger.h"
#include "record_format.h"

#define CMD_ENDP	0x02
#define DATA_ENDP	0x86
#define REPLY_ENDP	0x88

#define TIMEOUT 100
#define DATA_TIMEOUT 5000


#define REQ_TYPE_TO_DEV		(0x0 << 0)
#define REQ_TYPE_TO_IFACE	(0x1 << 0)
#define REQ_TYPE_TO_ENDP	(0x2 << 0)

#define REQ_TYPE_VENDOR		(0x2 << 5)

#define REQ_TYPE_HOST2DEV	(0x0 << 7)
#define REQ_TYPE_DEV2HOST	(0x1 << 7)

#define VERSION_REG		0x01
#define CLOCKRATE_REG		0x02
#define CAPCTL_REG		0x03
#define   CAPCTL_CAPTURE_EN	  (1<<0)
#define   CAPCTL_COUNT_EN	  (1<<1)
#define   CAPCTL_RESET_CNT	  (1<<2)
#define STROBE_REG		0x04
#define DELTA_REG		0x05


void timetagger::reset()
{
	stop_capture();
	flush_fx2_fifo();
	needs_flush = true;
}

uint32_t timetagger::read_reg(uint16_t reg)
{
	int ret, transferred;
	uint8_t buffer[] = { 0xAA, 0x00,
		(uint8_t) (reg >> 0),
		(uint8_t) (reg >> 8),
		0, 0, 0, 0 };

	if ( (ret = libusb_bulk_transfer(dev, CMD_ENDP, buffer, 8, &transferred, TIMEOUT)) )
		fprintf(stderr, "Failed sending request: %d\n", ret);

	if ( (ret = libusb_bulk_transfer(dev, REPLY_ENDP, buffer, 4, &transferred, TIMEOUT)) )
		fprintf(stderr, "failed receiving reply: %d\n", ret);

	if (transferred != 4)
		fprintf(stderr, "Invalid response\n");

	regs[reg] = *((uint32_t*) buffer);
	return regs[reg];
}

void timetagger::write_reg(uint16_t reg, uint32_t val)
{
	int ret, transferred;
	uint8_t buffer[] = { 0xAA, 0x00,
		(uint8_t) (reg >> 0),
		(uint8_t) (reg >> 8),
		(uint8_t) (val >> 0),
		(uint8_t) (val >> 8),
		(uint8_t) (val >> 16),
		(uint8_t) (val >> 24),
	};

	if ( (ret = libusb_bulk_transfer(dev, CMD_ENDP, buffer, 8, &transferred, TIMEOUT)) )
		fprintf(stderr, "Failed sending request: %d\n", ret);

	if ( (ret = libusb_bulk_transfer(dev, REPLY_ENDP, buffer, 1, &transferred, TIMEOUT)) )
		fprintf(stderr, "failed receiving reply: %d\n", ret);

	if (transferred != 1)
		fprintf(stderr, "No response\n");

	regs[reg] = buffer[0];
}

void timetagger::start_capture()
{
	// Don't start capture until flush has finished
	while (needs_flush)
		sleep(1);
	write_reg(0x3, regs[0x3] | CAPCTL_CAPTURE_EN | CAPCTL_COUNT_EN);
}

void timetagger::stop_capture()
{
	write_reg(0x3, regs[0x3] & ~CAPCTL_CAPTURE_EN);
}

void timetagger::reset_counter()
{
	write_reg(0x3, (regs[0x3] | CAPCTL_RESET_CNT) & ~CAPCTL_COUNT_EN);
	write_reg(0x3, regs[0x3] & ~CAPCTL_RESET_CNT);
}

unsigned int timetagger::get_record_count()
{
	return read_reg(0x6);
}

unsigned int timetagger::get_lost_record_count()
{
	return read_reg(0x7);
}

void timetagger::set_send_window(unsigned int records)
{
	unsigned int bytes = RECORD_LENGTH*records;
	if (bytes > 512) {
		fprintf(stderr, "Error: Send window too large\n");
		return;
	}

	int res = libusb_control_transfer(dev,
		REQ_TYPE_TO_DEV | REQ_TYPE_VENDOR | REQ_TYPE_HOST2DEV,
		0x01, bytes, 0, NULL, 0, 1);
	if (res != 0)
		fprintf(stderr, "Error setting send window size: %d\n", res);

	send_window = records;
}

void timetagger::flush_fx2_fifo() {
	// Request FIFO flush
	int res = libusb_control_transfer(dev, REQ_TYPE_VENDOR, 0x02, 0, 0, NULL, 0, 0);
	if (res != 0)
		fprintf(stderr, "Error requesting FX2 FIFO flush: %d\n", res);
}

void timetagger::readout_handler::do_flush() {
	int transferred = 0;
	do {
		uint8_t buffer[510];
		usleep(5000); // Give plenty of time for FPGA to fill FIFOs
		libusb_bulk_transfer(dev, DATA_ENDP, buffer, 510, &transferred, 10);
		//fprintf(stderr, "Flushed %d bytes\n", transferred);
	} while (transferred > 0);
	needs_flush = false;
}

void timetagger::readout_handler::operator()()
{
	uint8_t* buffer = new uint8_t[510];

	do {
		if (needs_flush)
			do_flush();

		int transferred, res;
		res = libusb_bulk_transfer(dev, DATA_ENDP,
			buffer, 510,
			&transferred, data_timeout);
		//fprintf(stderr, "Read %d bytes\n", transferred);
		if (!res || res == LIBUSB_ERROR_OVERFLOW) {
			if (transferred % RECORD_LENGTH != 0)
				fprintf(stderr, "Warning: Received partial record.");
			cb(buffer, transferred);
		} else if (res == LIBUSB_ERROR_TIMEOUT) {
			// Ignore timeouts, we just need to fall through to the
			// needs_flush check every once in a while
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

void timetagger::start_readout() 
{
	assert(readout_thread == 0);
	readout_handler readout(dev, data_cb, needs_flush, send_window);
	readout_thread = boost::shared_ptr<boost::thread>(new boost::thread(readout));
}

void timetagger::stop_readout() {
	assert(readout_thread != 0);
	readout_thread->interrupt();
	readout_thread->join();
}


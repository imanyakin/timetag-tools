// vim: set fileencoding=utf-8 noet :

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

// Enable for protocol level dumps
//#define DEBUG

#include <cstdio>
#include <cstring>
#include "timetagger.h"
#include "record_format.h"

#define CMD_ENDP	0x02
#define DATA_ENDP	0x86
#define REPLY_ENDP	0x88

#define TIMEOUT 500


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
#define REC_COUNTER_REG		0x06
#define LOST_COUNTER_REG	0x07
#define REC_FIFO_REG		0x08

#define SEQ_REG			0x20
#define SEQ_CLOCKRATE_REG	0x21
#define SEQ_CONFIG_BASE		0x28

timetagger::timetagger(libusb_device_handle* dev, data_cb_t& data_cb) :
	dev(dev),
	needs_flush(false),
	data_cb(data_cb)
{
	libusb_claim_interface(dev, 0);
	// Set send window to maximum value
	set_send_window(512/RECORD_LENGTH);

	// Start things off with sane defaults
	write_reg(0x0, 0x00); // Possibly unjam register manager
	flush_fx2_fifo();
	write_reg(CAPCTL_REG, 0x00);
	write_reg(STROBE_REG, 0x0f); // Strobe channel control
	write_reg(DELTA_REG, 0x0f); // Delta channel control

	// Update register cache
	for (int i=1; i<TIMETAG_NREGS; i++)
		read_reg(i);
}

timetagger::~timetagger()
{
	stop_readout();
	libusb_release_interface(dev, 0);
}

void timetagger::flush_fifo()
{
	// Clear sample FIFO
	write_reg(REC_FIFO_REG, regs[REC_FIFO_REG] | 0x1);
	write_reg(REC_FIFO_REG, regs[REC_FIFO_REG] & ~0x1);
}

void timetagger::reset()
{
	stop_capture();
	flush_fx2_fifo();
	needs_flush = true;
}

uint32_t timetagger::reg_cmd(bool write, uint16_t reg, uint32_t val)
{
	int ret, transferred;
	uint8_t buffer[] = { 0xAA, (uint8_t) write,
		(uint8_t) (reg >> 0),
		(uint8_t) (reg >> 8),
		(uint8_t) (val >> 0),
		(uint8_t) (val >> 8),
		(uint8_t) (val >> 16),
		(uint8_t) (val >> 24),
	};

#ifdef DEBUG
	fprintf(stderr, "%s reg %04x = %08x; ", write ? "write" : "read", reg, val);
#endif

	if ( (ret = libusb_bulk_transfer(dev, CMD_ENDP, buffer, 8, &transferred, TIMEOUT)) ) {
		fprintf(stderr, "Failed to send request: %d\n", ret);
		throw std::runtime_error("Failed to send request");
	}

	if ( (ret = libusb_bulk_transfer(dev, REPLY_ENDP, buffer, 4, &transferred, TIMEOUT)) ) {
		fprintf(stderr, "Failed to receive reply: %d\n", ret);
		throw std::runtime_error("Failed to receive reply");
	}

#ifdef DEBUG
	fprintf(stderr, "reply: ");
	for (int i=0; i<transferred; i++)
		fprintf(stderr, " %02x ", buffer[i]);
	fprintf(stderr, "\n");
#endif

	if (transferred != 4) {
		fprintf(stderr, "Invalid reply (length=%d)\n", transferred);
		throw std::runtime_error("Invalid reply");
	}

	regs[reg] = *((uint32_t*) buffer);
	return regs[reg];
}

uint32_t timetagger::read_reg(uint16_t reg)
{
	return reg_cmd(false, reg, 0x0);
}

void timetagger::write_reg(uint16_t reg, uint32_t val)
{
	reg_cmd(true, reg, val);
}

void timetagger::set_strobe_operate(unsigned int channel, bool enabled)
{
	if (enabled)
		write_reg(STROBE_REG, regs[STROBE_REG] | (1 << channel));
	else
		write_reg(STROBE_REG, regs[STROBE_REG] & ~(1 << channel));
}

bool timetagger::get_strobe_operate(unsigned int channel)
{
	return read_reg(STROBE_REG) & (1<<channel);
}

void timetagger::set_delta_operate(unsigned int channel, bool enabled)
{
	if (enabled)
		write_reg(DELTA_REG, regs[DELTA_REG] | (1 << channel));
	else
		write_reg(DELTA_REG, regs[DELTA_REG] & ~(1 << channel));
}

bool timetagger::get_delta_operate(unsigned int channel)
{
	return read_reg(DELTA_REG) & (1<<channel);
}

unsigned int timetagger::get_version()
{
	return read_reg(VERSION_REG);
}

unsigned int timetagger::get_clockrate()
{
	return read_reg(CLOCKRATE_REG);
}

void timetagger::start_capture()
{
	// Don't start capture until flush has finished
	while (needs_flush)
		sleep(1);
	write_reg(CAPCTL_REG, regs[CAPCTL_REG] | CAPCTL_CAPTURE_EN | CAPCTL_COUNT_EN);
}

void timetagger::stop_capture()
{
	write_reg(CAPCTL_REG, regs[CAPCTL_REG] & ~CAPCTL_CAPTURE_EN);
}

void timetagger::reset_counter()
{
	write_reg(CAPCTL_REG, (regs[CAPCTL_REG] | CAPCTL_RESET_CNT) & ~CAPCTL_COUNT_EN);
	write_reg(CAPCTL_REG, regs[CAPCTL_REG] & ~CAPCTL_RESET_CNT);
}

unsigned int timetagger::get_record_count()
{
	return read_reg(REC_COUNTER_REG);
}

unsigned int timetagger::get_lost_record_count()
{
	return read_reg(LOST_COUNTER_REG);
}

void timetagger::set_global_sequencer_operate(bool operate)
{
	if (operate)
		write_reg(SEQ_REG, regs[SEQ_REG] | 0x1);
	else
		write_reg(SEQ_REG, regs[SEQ_REG] & ~0x1);
}

bool timetagger::get_global_sequencer_operate()
{
	return read_reg(SEQ_REG) & 0x1;
}

void timetagger::reset_sequencer()
{
	write_reg(SEQ_REG, 0x2);
	write_reg(SEQ_REG, 0x0);
}

unsigned int timetagger::get_seq_clockrate()
{
	return read_reg(SEQ_CLOCKRATE_REG);
}

void timetagger::set_seqchan_operate(unsigned int seq, bool operate)
{
	unsigned int base_reg = SEQ_CONFIG_BASE + 0x8*seq;
	if (operate)
		write_reg(base_reg, regs[base_reg] | 0x1);
	else
		write_reg(base_reg, regs[base_reg] & ~0x1);
}

bool timetagger::get_seqchan_operate(unsigned int seq)
{
	unsigned int base_reg = SEQ_CONFIG_BASE + 0x8*seq;
	return read_reg(base_reg) & 0x1;
}

void timetagger::set_seqchan_initial_state(unsigned int seq, bool initial_state)
{
	unsigned int base_reg = SEQ_CONFIG_BASE + 0x8*seq;
	if (initial_state)
		write_reg(base_reg, regs[base_reg] | 0x02); 
	else
		write_reg(base_reg, regs[base_reg] & ~0x02); 
}

void timetagger::set_seqchan_initial_count(unsigned int seq, uint32_t initial_count)
{
	unsigned int base_reg = SEQ_CONFIG_BASE + 0x8*seq;
	write_reg(base_reg+1, initial_count);
}

void timetagger::set_seqchan_low_count(unsigned int seq, uint32_t low_count)
{
	unsigned int base_reg = SEQ_CONFIG_BASE + 0x8*seq;
	write_reg(base_reg+2, low_count);
}

void timetagger::set_seqchan_high_count(unsigned int seq, uint32_t high_count)
{
	unsigned int base_reg = SEQ_CONFIG_BASE + 0x8*seq;
	write_reg(base_reg+3, high_count);
}

bool timetagger::get_seqchan_initial_state(unsigned int seq)
{
	unsigned int base_reg = SEQ_CONFIG_BASE + 0x8*seq;
	return read_reg(base_reg) & 0x02;
}

uint32_t timetagger::get_seqchan_initial_count(unsigned int seq)
{
	unsigned int base_reg = SEQ_CONFIG_BASE + 0x8*seq;
	return read_reg(base_reg+1);
}

uint32_t timetagger::get_seqchan_low_count(unsigned int seq)
{
	unsigned int base_reg = SEQ_CONFIG_BASE + 0x8*seq;
	return read_reg(base_reg+2);
}

uint32_t timetagger::get_seqchan_high_count(unsigned int seq)
{
	unsigned int base_reg = SEQ_CONFIG_BASE + 0x8*seq;
	return read_reg(base_reg+3);
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
	uint8_t buffer[512];
        // Flush data FIFO
	do {
		usleep(10000); // Give plenty of time for FPGA to fill FIFOs
		libusb_bulk_transfer(dev, DATA_ENDP, buffer, 512, &transferred, 10);
	} while (transferred > 0);

        // Flush command reply FIFO
	do {
		usleep(10000); // Give plenty of time for FPGA to fill FIFOs
                libusb_bulk_transfer(dev, REPLY_ENDP, buffer, 512, &transferred, 10);
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
			// Ignore timeouts so we can check needs_flush
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


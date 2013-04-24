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

#include <assert.h>
#include <unistd.h>
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

// This is a common theme when handling libusb completions
void completed_cb(libusb_transfer *transfer) {
	int *completed = (int *) transfer->user_data;
	*completed = 1;
}

timetagger::timetagger(libusb_context* ctx, libusb_device_handle* dev, data_cb_t data_cb) :
	ctx(ctx),
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
	int ret;
	uint8_t buffer[] = { 0xAA, (uint8_t) write,
		(uint8_t) (reg >> 0),
		(uint8_t) (reg >> 8),
		(uint8_t) (val >> 0),
		(uint8_t) (val >> 8),
		(uint8_t) (val >> 16),
		(uint8_t) (val >> 24),
	};

	int completed = 0;
	libusb_transfer* transfer = libusb_alloc_transfer(0);
	if (transfer == NULL) {
		fprintf(stderr, "Error allocating transfer for register command\n");
		throw std::runtime_error("Error allocating transfer for register command\n");
	}
	
#ifdef DEBUG
	fprintf(stderr, "%s reg %04x = %08x; ", write ? "write" : "read", reg, val);
#endif

	// Submit request
	libusb_fill_bulk_transfer(transfer, dev, CMD_ENDP, buffer, 8,
			 	  completed_cb, &completed, TIMEOUT);
	ret = libusb_submit_transfer(transfer);
	if (ret) {
		fprintf(stderr, "Failed to send request: %d\n", ret);
		throw std::runtime_error("Failed to send request");
	}

	while (!completed)
		libusb_handle_events_completed(ctx, &completed);

	// Read reply
	completed = 0;
	libusb_fill_bulk_transfer(transfer, dev, REPLY_ENDP, buffer, 4,
			 	  completed_cb, &completed, TIMEOUT);
	ret = libusb_submit_transfer(transfer);
	if (ret) {
		fprintf(stderr, "Failed to receive reply: %d\n", ret);
		throw std::runtime_error("Failed to receive reply");
	}

	while (!completed)
		libusb_handle_events_completed(ctx, &completed);

#ifdef DEBUG
	fprintf(stderr, "reply: ");
	for (int i=0; i<transfer->actual_length; i++)
		fprintf(stderr, " %02x ", buffer[i]);
	fprintf(stderr, "\n");
#endif

	if (transfer->actual_length != 4) {
		fprintf(stderr, "Invalid reply (length=%d)\n", transfer->actual_length);
		throw std::runtime_error("Invalid reply");
	}

	libusb_free_transfer(transfer);
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

bool timetagger::get_capture_en()
{
	return read_reg(CAPCTL_REG) & CAPCTL_CAPTURE_EN;
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

	libusb_transfer *transfer = libusb_alloc_transfer(0);
	if (transfer == NULL) {
		fprintf(stderr, "Error allocating transfer for flush\n");
		throw std::runtime_error("Error allocating transfer for flush\n");
	}

	int completed = 0;
	unsigned char buffer[8];
	libusb_fill_control_setup(buffer,
				  REQ_TYPE_TO_DEV | REQ_TYPE_HOST2DEV | REQ_TYPE_VENDOR,
				  0x01, bytes, 0, 0);
	libusb_fill_control_transfer(transfer, dev, buffer,
				     completed_cb, &completed, 0);

	int res = libusb_submit_transfer(transfer);
	if (res != 0)
		fprintf(stderr, "Error requesting window size change: %d\n", res);

	while (!completed)
		libusb_handle_events_completed(ctx, &completed);

	libusb_free_transfer(transfer);
	send_window = records;
}

// Request FIFO flush
void timetagger::flush_fx2_fifo() {
	libusb_transfer *transfer = libusb_alloc_transfer(0);
	if (transfer == NULL) {
		fprintf(stderr, "Error allocating transfer for flush\n");
		throw std::runtime_error("Error allocating transfer for flush\n");
	}

	int completed = 0;
	unsigned char buffer[8];
	libusb_fill_control_setup(buffer, REQ_TYPE_VENDOR, 0x02, 0, 0, 0);
	libusb_fill_control_transfer(transfer, dev, buffer,
				     completed_cb, &completed, 0);

	int res = libusb_submit_transfer(transfer);
	if (res != 0)
		fprintf(stderr, "Error requesting FX2 FIFO flush: %d\n", res);

	while (!completed)
		libusb_handle_events_completed(ctx, &completed);
	libusb_free_transfer(transfer);
}

void timetagger::do_flush()
{
	uint8_t buffer[512];
	int completed;
	libusb_transfer *transfer = libusb_alloc_transfer(0);
	if (transfer == NULL) {
		fprintf(stderr, "Error allocating transfer for flush\n");
		throw std::runtime_error("Error allocating transfer for flush\n");
	}

        // Flush data FIFO
	libusb_fill_bulk_transfer(transfer, dev, DATA_ENDP, buffer, 512, completed_cb, &completed, 10);
	do {
		usleep(10000); // Give plenty of time for FPGA to fill FIFOs
		completed = 0;
		int ret = libusb_submit_transfer(transfer);
		if (ret != 0)
			fprintf(stderr, "Error flushing data FIFO: %d\n", ret);
		while (!completed)
			libusb_handle_events_completed(ctx, &completed);
	} while (transfer->actual_length > 0);

        // Flush command reply FIFO
	libusb_fill_bulk_transfer(transfer, dev, REPLY_ENDP, buffer, 512, completed_cb, &completed, 10);
	do {
		usleep(10000); // Give plenty of time for FPGA to fill FIFOs
		completed = 0;
		int ret = libusb_submit_transfer(transfer);
		if (ret != 0)
			fprintf(stderr, "Error flushing reply FIFO: %d\n", ret);
		while (!completed)
			libusb_handle_events_completed(ctx, &completed);
	} while (transfer->actual_length > 0);

	libusb_free_transfer(transfer);
	needs_flush = false;
}

void timetagger::readout_handler()
{
	const int data_timeout = 500;
	int failed_xfers = 0;
	uint8_t* buffer = new uint8_t[510];

	libusb_transfer *transfer = libusb_alloc_transfer(0);
	if (transfer == NULL) {
		fprintf(stderr, "Error allocating transfer for readout\n");
		throw std::runtime_error("Error allocating transfer for readout\n");
	}
	
	int completed = 0;
	libusb_fill_bulk_transfer(transfer, dev, DATA_ENDP, buffer, 510,
			 	  completed_cb, &completed, data_timeout);

	// Try bumping up ourselves into the FIFO scheduler 
	sched_param sp;
	sp.sched_priority = 50;
	if (sched_setscheduler(0, SCHED_FIFO, &sp))
		fprintf(stderr, "FIFO scheduling failed\n");

	while (!_stop_readout) {
		completed = 0;
		if (needs_flush)
			do_flush();

		int res = libusb_submit_transfer(transfer);
		if (res) {
			fprintf(stderr, "Failed to send request: %d\n", res);
			throw std::runtime_error("Failed to send request");
		}

		while (!completed && !_stop_readout)
			libusb_handle_events_completed(ctx, &completed);

		if (_stop_readout) break;

		//fprintf(stderr, "Read %d bytes\n", transfer->actual_length);
		if (!res || res == LIBUSB_ERROR_OVERFLOW) {
			if (transfer->actual_length % RECORD_LENGTH != 0)
				fprintf(stderr, "Warning: Received partial record.");
			if (transfer->actual_length > 0)
				data_cb(buffer, transfer->actual_length);
			failed_xfers = 0;
		} else if (res == LIBUSB_ERROR_TIMEOUT) {
			// Ignore timeouts so we can check needs_flush
		} else {
			fprintf(stderr, "Readout transfer failed: %d\n", res);
			failed_xfers++;
			if (failed_xfers > 1000) {
				fprintf(stderr, "Too many failed transfers. Read-out stopped\n");
				break;
			}
		}
	}

	libusb_free_transfer(transfer);
	delete [] buffer;
}

void timetagger::start_readout() 
{
	assert(readout_thread == 0);
	_stop_readout = false;
	readout_thread = std::shared_ptr<std::thread>(new std::thread(&timetagger::readout_handler, this));
}

void timetagger::stop_readout() {
	assert(readout_thread != 0);
	_stop_readout = true;
	readout_thread->join();
}


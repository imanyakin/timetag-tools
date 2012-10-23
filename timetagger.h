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


#ifndef _TIMETAGGER_H
#define _TIMETAGGER_H

#include <libusb.h>
#include <vector>
#include <array>
#include <memory>
#include <thread>

#include "record_format.h"

#define TIMETAG_NREGS 0x50

class timetagger {
public:
	typedef std::function<void (const uint8_t* buffer, size_t length)> data_cb_t;

private:
	libusb_device_handle* dev;
	std::shared_ptr<std::thread> readout_thread;
	bool _stop_readout;
	unsigned int data_timeout; // milliseconds
	bool needs_flush;
	unsigned int send_window; // In records
	
	// Register cache
	uint32_t regs[TIMETAG_NREGS];

	uint32_t reg_cmd(bool write, uint16_t reg, uint32_t val);
	uint32_t read_reg(uint16_t reg);
	void write_reg(uint16_t reg, uint32_t val);
	void flush_fx2_fifo();
	void readout_handler();
	void do_flush();

public:
	data_cb_t data_cb;

	timetagger(libusb_device_handle* dev, data_cb_t data_cb);
	~timetagger();
	
	void reset();
	void set_send_window(unsigned int records);

	unsigned int get_version();
	unsigned int get_clockrate();

	void set_strobe_operate(unsigned int channel, bool enabled);
	bool get_strobe_operate(unsigned int channel);
	void set_delta_operate(unsigned int channel, bool enabled);
	bool get_delta_operate(unsigned int channel);

	void start_readout();
	void stop_readout();

	void start_capture();
	void stop_capture();
	bool get_capture_en();
	void reset_counter();
	void flush_fifo();

	unsigned int get_seq_clockrate();
	unsigned int get_record_count();
	unsigned int get_lost_record_count();

	void set_global_sequencer_operate(bool operate);
	bool get_global_sequencer_operate();
	void reset_sequencer();
	void set_seqchan_operate(unsigned int seq, bool operate);
	bool get_seqchan_operate(unsigned int seq);
	void set_seqchan_initial_state(unsigned int seq, bool initial_state);
	bool get_seqchan_initial_state(unsigned int seq);
	void set_seqchan_initial_count(unsigned int seq, uint32_t initial_count);
	uint32_t get_seqchan_initial_count(unsigned int seq);
	void set_seqchan_low_count(unsigned int seq, uint32_t low_count);
	uint32_t get_seqchan_low_count(unsigned int seq);
	void set_seqchan_high_count(unsigned int seq, uint32_t high_count);
	uint32_t get_seqchan_high_count(unsigned int seq);
};

#endif


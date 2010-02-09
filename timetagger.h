#ifndef _TIMETAGGER_H
#define _TIMETAGGER_H

#include <libusb.h>
#include <vector>
#include <tr1/array>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include "record_format.h"

class timetagger {
public:
	struct data_cb_t {
		virtual void operator()(const uint8_t* buffer, size_t length)=0;
	};

private:
	struct readout_handler {
		libusb_device_handle* dev;
		timetagger::data_cb_t& cb;
		const unsigned int data_timeout; // milliseconds
		bool& needs_flush;
		unsigned int& send_window;

		readout_handler(libusb_device_handle* dev, timetagger::data_cb_t& cb,
			bool& needs_flush, unsigned int& send_window) :
			dev(dev), cb(cb), data_timeout(500),
			needs_flush(needs_flush), send_window(send_window) { }
		void operator()();
	private:
		void do_flush();
	};

private:
	typedef std::vector<uint8_t> cmd_data;
	libusb_device_handle* dev;
	boost::shared_ptr<boost::thread> readout_thread;
	bool needs_flush;
	unsigned int send_window; // In records
	
	void send_simple_command(uint8_t mask, cmd_data data);
	void flush_fx2_fifo();

public:
	data_cb_t& data_cb;

	timetagger(libusb_device_handle* dev, data_cb_t& data_cb) :
		dev(dev),
		needs_flush(false),
		data_cb(data_cb)
	{
		libusb_claim_interface(dev, 0);
		// Set send window to maximum value
		set_send_window(512/RECORD_LENGTH);
	}

	~timetagger()
	{
		stop_readout();
		libusb_release_interface(dev, 0);
	}
	
	void reset();
	void set_send_window(unsigned int records);

	void start_readout();
	void stop_readout();

	void start_capture();
	void stop_capture();
	void reset_counter();

	void pulseseq_start(std::tr1::array<bool, 4> units);
	void pulseseq_stop(std::tr1::array<bool, 4> units);
	void pulseseq_set_low_count(char seq_mask, uint32_t count);
	void pulseseq_set_high_count(char seq_mask, uint32_t count);
	void pulseseq_set_initial_count(char seq_mask, uint32_t count);
	void pulseseq_set_initial_state(char seq_mask, bool state);
};

#endif


#include <vector>
#include <iostream>
#include <boost/program_options.hpp>
#include "record.h"

namespace po = boost::program_options;

int main(int argc, char** argv) {
	po::options_description desc("Allowed options");
	desc.add_options()
		("help,h", "Display help message")
		("strobe-on,s", po::value<unsigned int>(), "include only records with strobe channel N active")
		("delta-on,d", po::value<unsigned int>(), "include only records with delta channel N active")
		("start-time,t", po::value<uint64_t>(), "start at timestamp TIME")
		("end-time,T", po::value<uint64_t>(), "end at timestamp TIME")
		("skip-records,r", po::value<unsigned int>(), "skip N records");

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	int strobe_on=-1, delta_on=-1;
	std::bitset<4> delta_status;
	uint64_t start_time = 0,  end_time = 1ULL << 63;
	unsigned int skip_records = 0;

	if (vm.count("help")) {
		std::cout << desc << "\n";
		return 0;
	}

	if (vm.count("strobe-on"))
		strobe_on = vm["strobe-on"].as<unsigned int>();

	if (vm.count("delta-on"))
		delta_on = vm["delta-on"].as <unsigned int>();

	if (vm.count("start-time"))
		start_time = vm["start-time"].as<uint64_t>();

	if (vm.count("end-time"))
		end_time = vm["end-time"].as<uint64_t>();
	
	if (vm.count("skip-records"))
		skip_records = vm["skip-records"].as<unsigned int>();


	record_stream stream(0);
	unsigned int i=0;
	while (true) {
		record r = stream.get_record();
		i++;

		if (r.get_type() == record::DELTA) {
			delta_status = r.get_channels();
			continue;
		}
		
		if (r.get_time() > end_time) break;
		if (r.get_time() < start_time) continue;
		if (i <= skip_records) continue;

		std::bitset<4> chans = r.get_channels();
		if (strobe_on != -1 && !chans[strobe_on]) continue;
		if (delta_on != -1 && !delta_status[delta_on]) continue;
	
		write_record(1, r);
	}
}


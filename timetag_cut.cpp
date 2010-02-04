#include <vector>
#include <boost/program_options.hpp>
#include "record.h"

namespace po = boost::program_options;

int main(int argc, char** argv) {
	po::options_description desc("Allowed options");
	desc.add_options()
		("help,h", "Display help message")
		("channels,c", po::value<unsigned int>(), "include only channels N")
		("start-time,t", po::value<uint64_t>(), "start at timestamp TIME")
		("end-time,T", po::value<uint64_t>(), "end at timestamp TIME")
		("skip-records,r", po::value<unsigned int>(), "skip N records");

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	std::vector<unsigned int> channels = { 0, 1, 2, 3 };
	uint64_t start_time = 0,  end_time = 1ULL << 63;
	unsigned int skip_records = 0;

	if (vm.count("channels"))
		channels = vm["channels"].as< std::vector<unsigned int> >();

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
		
		if (r.get_time() > end_time) break;
		if (r.get_time() < start_time) continue;
		if (i <= skip_records) continue;

		std::bitset<4> chans = r.get_channels();
		auto c=channels.begin();
		for (; c != channels.end(); c++)
			if (channels[*c]) break;
		if (c == channels.end()) continue;
		
		write_record(1, r);
	}
}


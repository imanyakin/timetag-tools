#include "record.h"
#include <boost/python.hpp>
using namespace boost::python;

BOOST_PYTHON_MODULE(pytimetag) {
	class_<record_stream>("Stream", init<int>())
		.def("get_record", &record_stream::get_record);

	class_<record>("Record", no_init)
		.add_property("time", &record::get_time)
		.add_property("type", &record::get_type)
		.add_property("wrap_flag", &record::get_wrap_flag)
		.add_property("lost_flag", &record::get_lost_flag)
		.add_property("channels", &record::get_channels);
}


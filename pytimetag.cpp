#include "record.h"
#include <boost/python.hpp>
using namespace boost::python;

void eof_translator(end_stream const& x) {
	PyErr_Format(PyExc_EOFError, "End of stream");
}

bool get_channel(record r, unsigned int i) {
	return r.get_channels()[i];
}

BOOST_PYTHON_MODULE(pytimetag) {
	register_exception_translator<end_stream>(eof_translator);
	enum_<record::type>("record_type")
		.value("STROBE", record::type::STROBE)
		.value("DELTA", record::type::DELTA);

	class_<record_stream>("Stream", init<int>())
		.def("get_record", &record_stream::get_record);

	class_<record>("Record", no_init)
		.add_property("time", &record::get_time)
		.add_property("type", &record::get_type)
		.add_property("wrap_flag", &record::get_wrap_flag)
		.add_property("lost_flag", &record::get_lost_flag)
		.def("get_channel", &get_channel);
}


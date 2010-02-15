PREFIX=/usr
CXXFLAGS=-Wall -std=c++0x -ggdb -I/usr/include/libusb-1.0
CC=g++

PROGS=timetag_acquire bin_photons photon_generator dump_records dump_photons timetag_cut dump_bins

all : ${PROGS}

timetag_acquire : timetag_acquire.o timetagger.o
	g++ -lusb-1.0 -lboost_thread -o$@ $+

timetag_cut : timetag_cut.o record.o
	g++ -lboost_program_options -o$@ $+
dump_photons : dump_photons.o record.o
dump_records : dump_records.o record.o
bin_photons : bin_photons.o record.o
dump_bins : dump_bins.o

install : all
	cp timetag_acquire ${PREFIX}/bin/timetag_acquire
	chmod ug+s ${PREFIX}/bin/timetag_acquire
	cp ${PROGS} ${PREFIX}/bin
	cp timetag_ui.py plot_bins hist_bins bin_photons_us ${PREFIX}/bin
	cp ringbuffer.py capture_pipeline.py timetag_interface.py ${PREFIX}/lib/pymodules/python2.6
	mkdir -p ${PREFIX}/share/timetag
	cp timetag_ui.glade output_channel.glade ${PREFIX}/share/timetag
	cp default.cfg ${PREFIX}/share/timetag
	git rev-parse HEAD > ${PREFIX}/share/timetag/timetag-tools-ver

clean :
	rm -f ${PROGS} *.o *.pyc


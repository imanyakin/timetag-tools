PREFIX=/usr
CXXFLAGS=-Wall -std=c++0x -ggdb -I/usr/include/libusb-1.0 -fPIC
CC=$(CXX)

PROGS=timetag_acquire bin_photons photon_generator dump_records dump_photons \
      timetag_cut dump_bins strip_bins matlab-dump

all : ${PROGS}

timetag_acquire : LDLIBS = -lusb-1.0 -lboost_thread
timetag_acquire : timetag_acquire.o timetagger.o
timetag_cut : LDLIBS = -lboost_program_options
timetag_cut : timetag_cut.o record.o
dump_photons : dump_photons.o record.o
dump_records : dump_records.o record.o
bin_photons : bin_photons.o record.o
dump_bins : dump_bins.o record.o
matlab-dump : matlab-dump.o record.o

pytimetag.so : pytimetag.o record.o
	g++ -shared ${CXXFLAGS} -lpython2.6 -lboost_python -o$@ $+
pytimetag.o : pytimetag.cpp
	g++ -c ${CXXFLAGS} -I/usr/include/python2.6 -o$@ $+


install : all
	cp timetag_acquire ${PREFIX}/bin/timetag_acquire
	chmod ug+s ${PREFIX}/bin/timetag_acquire
	cp ${PROGS} ${PREFIX}/bin
	cp timetag_ui.py plot_bins hist_bins bin_photons_us fret_eff ${PREFIX}/bin
	cp ringbuffer.py capture_pipeline.py timetag_interface.py ${PREFIX}/lib/pymodules/python2.6
	mkdir -p ${PREFIX}/share/timetag
	cp timetag_ui.glade output_channel.glade ${PREFIX}/share/timetag
	cp default.cfg ${PREFIX}/share/timetag
	git rev-parse HEAD > ${PREFIX}/share/timetag/timetag-tools-ver

install-pytimetag : pytimetag.so
	cp pytimetag.so ${PREFIX}/lib/python2.6
	cp timetag_matlab_export ${PREFIX}/bin

clean :
	rm -f ${PROGS} *.o *.pyc pytimetag.so

# For automatic header dependencies
.deps/%.d : %
	@mkdir -p .deps
	@makedepend  ${INCLUDES} -f - $< 2>/dev/null | sed 's,\($*\.o\)[ :]*,\1 $@ : ,g' >$@

SOURCES = $(wildcard *.cpp) $(wildcard *.c)
-include $(addprefix .deps/,$(addsuffix .d,$(SOURCES)))


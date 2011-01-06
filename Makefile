PREFIX=/usr
CXXFLAGS=-Wall -std=c++0x -ggdb -I/usr/include/libusb-1.0 -fPIC
CC=$(CXX)

PROGS=timetag_acquire bin_photons photon_generator dump_records dump_photons \
      timetag_cut dump_bins strip_bins matlab-dump extract_timestamps

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
extract_timestamps : extract_timestamps.o record.o

pytimetag.so : pytimetag.o record.o
	g++ -shared ${CXXFLAGS} -lpython2.6 -lboost_python -o$@ $+
pytimetag.o : pytimetag.cpp
	g++ -c ${CXXFLAGS} -I/usr/include/python2.6 -o$@ $+


install : ${PROGS}
	cp ${PROGS} ${PREFIX}/bin
	chmod ug+s ${PREFIX}/bin/timetag_acquire
	mkdir -p ${PREFIX}/share/timetag
	git rev-parse HEAD > ${PREFIX}/share/timetag/timetag-tools-ver
	make -Cui install

install-pytimetag : pytimetag.so
	cp pytimetag.so ${PREFIX}/lib/python2.6
	cp timetag_matlab_export ${PREFIX}/bin

clean :
	rm -f ${PROGS} *.o *.pyc pytimetag.so
	make -Cui clean

# For automatic header dependencies
.deps/%.d : %
	@mkdir -p .deps
	@cpp ${INCLUDES} -std=c++0x -MM $< > $@

SOURCES = $(wildcard *.cpp) $(wildcard *.c)
-include $(addprefix .deps/,$(addsuffix .d,$(SOURCES)))


PREFIX=/usr
CXXFLAGS=-Wall -std=gnu++0x -ggdb -I/usr/include/libusb-1.0 -fPIC $(EXTRA_FLAGS)
CC=$(CXX)

PROGS=timetag_acquire photon_generator timetag_dump \
      timetag_cut timetag_extract timetag_bin

all : ${PROGS}

timetag_acquire : LDLIBS = -lusb-1.0 -lboost_iostreams
timetag_acquire : EXTRA_FLAGS = -DWITH_DOMAIN_SOCKET
timetag_acquire : timetag_acquire.o timetagger.o
timetag_cut : LDLIBS = -lboost_program_options
timetag_cut : timetag_cut.o record.o
timetag_bin : timetag_bin.o record.o
timetag_dump : timetag_dump.o record.o
timetag_extract : timetag_extract.o record.o

install : ${PROGS} install-upstart-job
	cp ${PROGS} ${PREFIX}/bin
	chmod ug+s ${PREFIX}/bin/timetag_acquire
	mkdir -p ${PREFIX}/share/timetag
	git rev-parse HEAD > ${PREFIX}/share/timetag/timetag-tools-ver

install-upstart-job : timetag-acquire.conf
	cp $< /etc/init/timetag-acquire.conf

clean :
	rm -f ${PROGS} *.o
	python ui/setup.py clean

# For automatic header dependencies
.deps/%.d : %
	@mkdir -p .deps
	@cpp ${INCLUDES} -std=c++0x -MM $< > $@

SOURCES = $(wildcard *.cpp) $(wildcard *.c)
-include $(addprefix .deps/,$(addsuffix .d,$(SOURCES)))


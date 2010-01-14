PREFIX=/usr/local
CXXFLAGS=-std=c++0x -ggdb -I/usr/include/libusb-1.0
CC=g++

PROGS=timetag_acquire bin_photons photon_generator dump_photons

all : ${PROGS}

timetag_acquire : timetag_acquire.o timetagger.o
	g++ -lusb-1.0 -lboost_thread -o$@ $+

install :
	cp timetag_acquire ${PREFIX}/bin/timetag_acquire
	chmod ug+s ${PREFIX}/bin/timetag_acquire

clean :
	rm -f ${PROGS} *.o


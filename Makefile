PREFIX=/usr/local
CXXFLAGS=-std=c++0x -ggdb -I/usr/include/libusb-1.0
CC=g++

PROGS=acquire bin_photons photon_generator dump_photons

all : ${PROGS}

acquire : acquire.o timetagger.o
	g++ -lusb-1.0 -lboost_thread -o$@ $+

install :
	cp acquire ${PREFIX}/bin/acquire
	chmod ug+s ${PREFIX}/bin/acquire

clean :
	rm -f ${PROGS} *.o


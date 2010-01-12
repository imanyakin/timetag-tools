CXXFLAGS=-ggdb -I/usr/include/libusb-1.0
CC=g++

PROGS=acquire bin_photons photon_generator dump_photons

all : ${PROGS}

acquire : acquire.o timetagger.o
	g++ -lusb-1.0 -lboost_thread -o$@ $+

clean :
	rm -f ${PROGS} *.o


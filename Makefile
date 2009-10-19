PROGS=acquire bin_photons photon_generator dump_photons

all : ${PROGS}

acquire : acquire.cpp
	g++ -ggdb -lusb-1.0 -I /usr/include/libusb-1.0 -o$@ $+

clean :
	rm -f ${PROGS}

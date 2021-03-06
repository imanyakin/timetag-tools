PREFIX=/usr
CXXFLAGS=-Wall -std=gnu++0x -ggdb $(shell pkg-config --cflags libusb-1.0) -fPIC $(EXTRA_FLAGS)
LDLIBS=$(shell pkg-config --libs libusb-1.0) -lpthread
CC=$(CXX)

CPP_PROGS=timetag_acquire photon_generator timetag_dump \
      timetag_cut timetag_extract timetag_bin timetag_elide
PROGS=timetag-cli timetag-cat ${CPP_PROGS}

ifndef DEBUG
	CXXFLAGS+=-O
endif

all : ${PROGS}

timetag_acquire : LDLIBS += -lboost_iostreams -lzmq
timetag_acquire : EXTRA_FLAGS = -DWITH_DOMAIN_SOCKET
timetag_acquire : timetag_acquire.o timetagger.o
timetag_cut : LDLIBS += -lboost_program_options
timetag_cut : timetag_cut.o record.o
timetag_bin : LDLIBS += -lboost_program_options
timetag_bin : timetag_bin.o record.o
timetag_dump : timetag_dump.o record.o
timetag_extract : timetag_extract.o record.o
timetag_elide : timetag_elide.o record.o

.PHONY : install
install : install-exec install-udev install-passwd install-systemd

.PHONY : install-exec
install-exec : ${PROGS}
	cp ${PROGS} ${PREFIX}/bin
	chmod ug+s ${PREFIX}/bin/timetag_acquire
	mkdir -p ${PREFIX}/share/timetag
	git rev-parse HEAD > ${PREFIX}/share/timetag/timetag-tools-ver

.PHONY : install-passwd
install-passwd :
	adduser --system --group --disabled-login --home /var/run/timetag --shell /bin/false timetag

.PHONY : install-systemd
install-systemd : systemd/timetag-acquire.service install-udev
	cp $< /lib/systemd/system

.PHONY : install-upstart-job
install-upstart-job : timetag-acquire.conf install-udev
	@echo "Note that upstart support is deprecated. Use install-systemd if possible."
	cp $< /etc/init/timetag-acquire.conf

.PHONY : install-udev
install-udev : timetag-acquire.rules
	-rm -f /etc/init/timetag-acquire.conf # Ensure upstart job isn't installed as well
	-rm -f /etc/udev/rules.d/timetag-acquire.rules # Ensure old rules aren't present
	cp timetag-acquire.rules /etc/udev/rules.d/99-timetag-acquire.rules

clean :
	rm -f ${CPP_PROGS} *.o
	python ui/setup.py clean

# For automatic header dependencies
.deps/%.d : %
	@mkdir -p .deps
	@cpp ${INCLUDES} -std=c++0x -MM $< > $@

SOURCES = $(wildcard *.cpp) $(wildcard *.c)
-include $(addprefix .deps/,$(addsuffix .d,$(SOURCES)))


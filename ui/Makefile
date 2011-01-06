PREFIX=/usr

install : 
	cp timetag_ui.py ${PREFIX}/bin
	cp ringbuffer.py capture_pipeline.py timetag_interface.py ${PREFIX}/lib/pymodules/python2.6
	mkdir -p ${PREFIX}/share/timetag
	cp timetag_ui.glade output_channel.glade ${PREFIX}/share/timetag
	cp default.cfg ${PREFIX}/share/timetag

clean :
	rm -f *.pyc


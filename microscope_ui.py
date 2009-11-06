#!/usr/bin/python

import subprocess
import time
from collections import defaultdict
import threading
import gobject, gtk
from matplotlib.figure import Figure
from matplotlib.backends.backend_gtk import FigureCanvasGTK as FigureCanvas
import numpy

class CapturePipeline(object):
        def __init__(self, bin_length=10000000, output_file=None, points=1000):
                self.data = defaultdict(lambda: (numpy.zeros(self.points, dtype='u8'),  # bin start time
                                                 numpy.zeros(self.points, dtype='u4'))) # count
                self.indicies = defaultdict(lambda: 0)
                self.points = points

                self.bin_length = bin_length
                self.output_file = output_file
                self.update_cb = None

        def start(self):
                PIPE=subprocess.PIPE
                self.source = subprocess.Popen(['./photon_generator', str(100)], stdout=PIPE)
                src = self.source.stdout
                
                if self.output_file:
                        self.tee = subprocess.Popen(['tee', self.output_file], stdin=src, stdout=PIPE)
                        src = self.tee.stdout
                else:
                        self.tee = None

                self.binner = subprocess.Popen(['./bin_photons', str(self.bin_length)], stdin=src, stdout=PIPE)
                self.listener = threading.Thread(name='Data Listener', target=self.listen)
                self.listener.daemon = True
                self.listener.start()

        def listen(self):
                for l in self.binner.stdout:
                        chan, start_time, count = map(int, l.split())
                        times, counts = self.data[chan]
                        times[self.indicies[chan]] = start_time / 150e6
                        counts[self.indicies[chan]] = count
                        if self.update_cb: self.update_cb()

                        self.indicies[chan] += 1
                        self.indicies[chan] %= self.points
                        #print self.indicies[chan], self.data[chan]

        def stop(self):
                self.source.terminate()

                
class MainWindow(object):
        def do_timeout2(self):
                chan = 1
                x = numpy.arange(0, 3, 0.01)
                y = numpy.sin(x+self.dt)
                if not self.lines.has_key(chan):
                        self.lines[chan], = self.axes.plot(x, y) #, animated=True)
                        #self.figure.canvas.draw()
                else:
                        self.lines[chan].set_ydata(y)
                        #self.axes.draw_artist(self.lines[chan])

                self.figure.canvas.draw()
                self.dt += 0.1

        def update_plot(self):
                if not self.pipeline:
                        return False

                for chan in self.pipeline.data.keys():
                        times, counts = self.pipeline.data[chan]
                        if not self.lines.has_key(chan):
                                self.lines[chan], = self.axes.plot(times, counts) #, animated=True)
                                #self.axes.plot(times, counts)
                                #self.figure.canvas.draw()
                        else:
                                self.dt += 10
                                print numpy.max(times), numpy.min(times)
                                self.lines[chan].set_xdata(times+self.dt)
                                self.lines[chan].set_ydata(counts)
                                #self.axes.draw_artist(self.lines[chan])

                self.axes.autoscale_view()
                self.figure.canvas.draw()

        def __init__(self):
                self.dt = 0
                self.last_update = 0
                self.pipeline = None
                self.binned_data = None

                self.builder = gtk.Builder()
                self.builder.add_from_file('microscope_ui.glade')
                self.builder.connect_signals(self)
                self.win = self.builder.get_object('main_window')
                self.win.connect('destroy', gtk.main_quit)

                self.figure = Figure()
                self.axes = self.figure.add_subplot(111)
                #self.axes.xaxis.set_animated(True)
                self.lines = {}
                canvas = FigureCanvas(self.figure)
                self.builder.get_object('plot_container').pack_start(canvas)

                self.win.show_all()

        def start_pipeline(self):
                if self.pipeline:
                        raise "Tried to start a capture pipeline while one is already running"

                bin_length = 4000
                file = None
                if self.builder.get_object('file_output_action').props.active:
                        file = self.builder.get_object('data_file').get_filename()

                self.pipeline = CapturePipeline(bin_length, file)
                def update_cb():
                        if time.time() - self.last_update > 1.0/30:
                                gobject.idle_add(self.update_plot)
                                print 'update'
                                self.last_update = time.time()

                self.pipeline.update_cb = update_cb
                self.pipeline.start()

        def stop_pipeline(self):
                self.pipeline.stop()
                self.pipeline = None

        def running_toggled_cb(self, action):
                state = action.props.active
                sensitive_objects = [
                        'file_output_action',
                        'data_file',
                        'offset_time',
                        'high_time',
                        'low_time'
                ]
                for o in sensitive_objects:
                        self.builder.get_object(o).props.sensitive = not state

                if state:
                        self.start_pipeline()
                else:
                        self.stop_pipeline()

if __name__ == '__main__':
        gobject.threads_init()
        win = MainWindow()
        gtk.main()

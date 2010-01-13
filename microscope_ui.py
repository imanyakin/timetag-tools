#!/usr/bin/python

import logging
import time
import gobject, gtk
from matplotlib.figure import Figure
from matplotlib.backends.backend_gtk import FigureCanvasGTK as FigureCanvas
#from matplotlib.backends.backend_gtkagg import FigureCanvasGTKAgg as FigureCanvas
import numpy

from capture_pipeline import CapturePipeline, TestPipeline

logging.basicConfig(level=logging.DEBUG)

PULSESEQ_FREQ = 30e6

class MainWindow(object):
        def update_plot(self):
                if not self.pipeline:
                        self.update_pending = False
                        return False

                for n,times,counts in self.pipeline:
                        if not self.lines.has_key(n):
                                self.lines[n], = self.axes.plot(times, counts)#, animated=True)
                        else:
                                self.lines[n].set_data(times, counts)

                self.axes.relim()
                self.axes.autoscale_view(tight=True)
                self.figure.canvas.draw()
                self.last_update = time.time()
                self.update_pending = False
                return False

        def __init__(self):
                self.dt = 0
                self.last_update = 0
                self.update_pending = False
                self.pipeline = None
                self.binned_data = None

                self.builder = gtk.Builder()
                self.builder.add_from_file('microscope_ui.glade')
                self.builder.connect_signals(self)
                self.win = self.builder.get_object('main_window')
                self.win.connect('destroy', gtk.main_quit)

                self.figure = Figure()
                self.axes = self.figure.add_subplot(111)
                self.lines = {}
                canvas = FigureCanvas(self.figure)
                self.builder.get_object('plot_container').pack_start(canvas)

                self.win.show_all()

        def start_pipeline(self):
                if self.pipeline:
                        raise "Tried to start a capture pipeline while one is already running"

                bin_length = 10e6
                file = None
                if self.builder.get_object('file_output_action').props.active:
                        file = self.builder.get_object('data_file').get_filename()

                def update_cb():
                        if time.time() - self.last_update > 1.0/30:
                                gobject.idle_add(self.update_plot)
                                self.update_pending = True

                self.pipeline = CapturePipeline(bin_length, file)
                #self.pipeline = TestPipeline(100)
                self.pipeline.update_cb = update_cb
                self.pipeline.start()
                self.pipeline.tagger.start_capture()
                self.sync_pulse_seq()

        def stop_pipeline(self):
                self.pipeline.stop()
                self.pipeline = None

        def running_toggled_cb(self, action):
                state = action.props.active
                sensitive_objects = [
                        'file_output_action',
                        'data_file',
                ]
                for o in sensitive_objects:
                        self.builder.get_object(o).props.sensitive = not state

                if state:
                        self.start_pipeline()
                else:
                        self.stop_pipeline()

        def override_output(self, output, state=False):
                if not self.pipeline: return
                self.pipeline.tagger.stop_outputs([output])
                self.pipeline.tagger.set_initial_state(output, state)

        def program_pulse_seq(self, output, initial_state, initial_count, high_count, low_count):
                if not self.pipeline: return
                self.pipeline.tagger.set_initial_state(output, initial_state)
                self.pipeline.tagger.set_initial_count(output, initial_count*PULSESEQ_FREQ)
                self.pipeline.tagger.set_high_count(output, high_count*PULSESEQ_FREQ)
                self.pipeline.tagger.set_low_count(output, low_count*PULSESEQ_FREQ)
                self.pipeline.tagger.start_outputs([output])

        def sync_pulse_seq(self):
                """ Program the pulse sequencer to reflect the settings in the UI """
                output = self.builder.get_object("output_select").get_active()
                print output
                initial_state = self.builder.get_object("high_initial_state").get_current_value()
                initial_count = self.builder.get_object("offset_time").get_value()
                high_count = self.builder.get_object("high_time").get_value()
                low_count = self.builder.get_object("low_time").get_value()
                self.program_pulse_seq(output, initial_state, initial_count, high_count, low_count)

        def output_override_toggled_cb(self, action):
                override_enabled = self.builder.get_object('output_override_action').props.active
                if override_enabled:
                        state = self.builder.get_object('output_state_action').props.active
                        output = self.builder.get_object('output_select').get_active()
                        self.override_output(output, state)
                else:
                        self.sync_pulse_seq()

        def pulse_seq_changed_cb(self, *action):
                output = self.builder.get_object("output_select").get_active()
                print output
                self.sync_pulse_seq()

if __name__ == '__main__':
        gtk.gdk.threads_init()
        win = MainWindow()
        gtk.main()


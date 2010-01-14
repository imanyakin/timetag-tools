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

class OutputChannel(object):
        def __init__(self, main_window, output_n):
                self.main_window = main_window

                self.builder = gtk.Builder()
                self.builder.add_from_file('output_channel.glade')
                self.builder.connect_signals(self)

                self.icon = gtk.image_new_from_stock('Stop', gtk.ICON_SIZE_BUTTON)
                header = gtk.HBox()
                header.pack_start(self.icon)
                header.pack_start(gtk.Label('Channel %d' % output_n))
                header.show_all()
                self.notebook_label = header

                self.widget = self.builder.get_object('output_channel')
                self.output_n = output_n

        @property
        def tagger(self):
                if not self.main_window.pipeline: return None
                return self.main_window.pipeline.tagger

        def running_toggled_cb(self, action):
                active = action.props.active
                if active:
                        action.props.label = 'Running'
                        self.icon.set_from_stock('Play', gtk.ICON_SIZE_MENU)
                else:
                        action.props.label = 'Stopped'
                        self.icon.set_from_stock('Stop', gtk.ICON_SIZE_MENU)

                if not self.tagger: return
                if active:
                        self.tagger.start_outputs([self.output_n])
                else:
                        self.tagger.stop_outputs([self.output_n])

        def offset_time_value_changed_cb(self, adj):
                if not self.tagger: return
                count = adj.props.value * PULSESEQ_FREQ / 1e3
                self.tagger.set_initial_count(self.output_n, count)

        def high_time_value_changed_cb(self, adj):
                if not self.tagger: return
                count = adj.props.value * PULSESEQ_FREQ / 1e3
                self.tagger.set_high_count(self.output_n, count)

        def low_time_value_changed_cb(self, adj):
                if not self.tagger: return
                count = adj.props.value * PULSESEQ_FREQ / 1e3
                self.tagger.set_low_count(self.output_n, count)

        def initial_state_toggled_cb(self, action):
                state = action.props.active
                if state:
                        action.props.label = "High"
                else:
                        action.props.label = "Low"

                if not self.tagger: return
                self.tagger.set_initial_state(self.output_n, action.props.active)

        def override_enabled_changed_cb(self, action, current):
                override_enabled = bool(current)
                if override_enabled:
                        state = self.builder.get_object('output_state').props.active
                        self._do_override()
                else:
                        self.sync_pulse_seq()

        def override_state_toggled_cb(self, action):
                self._do_override()

        def _do_override(self):
                self.builder.get_object('running').props.active = False
                action = self.builder.get_object('override_state')
                state = action.props.active
                if state:
                        action.props.label = "High"
                        self.icon.set_from_stock('Up', gtk.ICON_SIZE_MENU)
                else:
                        action.props.label = "Low"
                        self.icon.set_from_stock('Down', gtk.ICON_SIZE_MENU)

                if not self.tagger: return
                self.tagger.set_initial_state(self.output_n, state)


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

                self.outputs = []
                notebook = self.builder.get_object('output_channels')
                for c in range(4):
                        chan = OutputChannel(self, c)
                        notebook.append_page(chan.widget, chan.notebook_label)
                        self.outputs.append(chan)

                self.figure = Figure()
                self.axes = self.figure.add_subplot(111)
                self.lines = {}
                canvas = FigureCanvas(self.figure)
                self.builder.get_object('plot_container').pack_start(canvas)

                self.win.show_all()

        def start_pipeline(self):
                if self.pipeline:
                        raise "Tried to start a capture pipeline while one is already running"

                file = None
                if self.builder.get_object('file_output_enabled').props.active:
                        file = self.builder.get_object('output_file').get_filename()

                def update_cb():
                        self.update_plot()
                        return True
                gobject.timeout_add(int(1000.0/10), update_cb)

                self.pipeline = CapturePipeline(output_file=file)
                #self.pipeline = TestPipeline(100)
                self.pipeline.start()

        def stop_pipeline(self):
                self.stop_readout()
                self.pipeline.stop()
                self.pipeline = None

        def pipeline_running_toggled_cb(self, action):
                state = action.props.active

                sensitive_objects = [ 'file_output_enabled', 'output_file', ]
                for o in sensitive_objects:
                        self.builder.get_object(o).props.sensitive = not state

                if state:
                        self.start_pipeline()
                else:
                        self.stop_pipeline()

        def start_readout(self):
                self.pipeline.tagger.start_capture()

        def stop_readout(self):
                self.pipeline.tagger.stop_capture()

        def readout_running_toggled_cb(self, action):
                if action.props.active:
                        self.start_readout()
                        action.props.label = "Running"       
                else:
                        self.stop_readout()
                        action.props.label = "Stopped"

        def start_outputs_activate_cb(self, action):
                self.pipeline.tagger.start_outputs([0,1,2,3])

        def stop_outputs_activate_cb(self, action):
                self.pipeline.tagger.stop_outputs([0,1,2,3])


if __name__ == '__main__':
        gtk.gdk.threads_init()
        win = MainWindow()
        gtk.main()


#!/usr/bin/python

import logging
from collections import defaultdict
import time
import os

import gobject, gtk
from matplotlib.figure import Figure
from matplotlib.backends.backend_gtk import FigureCanvasGTK as FigureCanvas
#from matplotlib.backends.backend_gtkagg import FigureCanvasGTKAgg as FigureCanvas

from capture_pipeline import CapturePipeline, TestPipeline

logging.basicConfig(level=logging.DEBUG)

PULSESEQ_FREQ = 30e6

glade_prefix='/usr/share/timetag'

class OutputChannel(object):
        sensitive_widgets = [
                'override_state', 'initial_state', 'running',
                'override_enabled', 'sequencer_enabled',
                'offset_time_spin', 'high_time_spin', 'low_time_spin',
        ]

        def __init__(self, main_window, output_n):
                self.main_window = main_window

                self.builder = gtk.Builder()
                self.builder.add_from_file(os.path.join(glade_prefix, 'output_channel.glade'))
                self.builder.connect_signals(self)

                self.icon = gtk.image_new_from_stock('Stop', gtk.ICON_SIZE_BUTTON)
                header = gtk.HBox()
                header.pack_start(self.icon)
                header.pack_start(gtk.Label('Channel %d' % output_n))
                header.show_all()
                self.notebook_label = header

                self._set_sensitivity(False)
                self.widget = self.builder.get_object('output_channel')
                self.output_n = output_n

        def pipeline_started(self):
                self._set_sensitivity(True)

        def pipeline_stopped(self):
                self._set_sensitivity(False)

        def _set_sensitivity(self, state):
                for w in self.__class__.sensitive_widgets:
                        self.builder.get_object(w).props.sensitive = state

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
                        initial_state = self.builder.get_object('initial_state').props.active
                        self.tagger.set_initial_state(self.output_n, initial_state)

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
        def __init__(self):
                self.update_rate = 10 # in Hertz
                self.pipeline = None

                self.builder = gtk.Builder()
                self.builder.add_from_file(os.path.join(glade_prefix, 'timetag_ui.glade'))
                self.builder.connect_signals(self)

                def quit(unused):
                        if self.pipeline:
                                self.pipeline.stop()
                        gtk.main_quit()
                self.win = self.builder.get_object('main_window')
                self.win.connect('destroy', quit)

		n_inputs = 4
		self.inputs = []
		table = self.builder.get_object('channel_stats')
		table.resize(n_inputs, 3)
		for c in range(n_inputs):
			label, photons, lost = gtk.Label(), gtk.Label(), gtk.Label()
			label.set_markup('<span size="large">Channel %d</span>' % c)
			table.attach(label, 0,1, c,c+1)
			table.attach(photons, 1,2, c,c+1)
			table.attach(lost, 2,3, c,c+1)
			self.inputs.append((photons, lost))

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

        def select_output_file_activate_cb(self, action):
                fc = gtk.FileChooserDialog('Select output file', self.win, gtk.FILE_CHOOSER_ACTION_SAVE,
                        (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL,  gtk.STOCK_OK, gtk.RESPONSE_OK))
                fc.props.do_overwrite_confirmation = True
                res = fc.run()
                fc.hide()
                if res == gtk.RESPONSE_OK:
                        self.builder.get_object('output_file').props.text = fc.get_filename()

        def update_plot(self):
                if not self.pipeline:
                        return False

                for n,times,counts in self.pipeline.bins():
                        if not self.lines.has_key(n):
                                self.lines[n], = self.axes.plot(times, counts)#, animated=True)
                        else:
                                self.lines[n].set_data(times, counts)

                # There must be a better way to do this
                self.axes.relim()
                self.axes.autoscale_view(scalex=True, scaley=False, tight=True)
                self.axes.autoscale_view(scalex=False, scaley=True, tight=False)
                _,ymax = self.axes.get_ylim()
                self.axes.set_ylim(ymin=0, ymax=1.1*ymax)
                xmin,xmax = self.axes.get_xlim()
                offset = 0
                readout_running = self.builder.get_object('readout_running').props.active
                if readout_running:
                        offset = time.time() - self.pipeline.last_bin_walltime
                self.axes.set_xlim(xmin=xmin+offset, xmax=xmax+offset)

                self.figure.canvas.draw()

        def update_indicators(self):
                display_type = self.builder.get_object('show_totals').props.current_value
                if display_type == 1:
                        self._update_rate_indicators()
                else:
                        self._update_total_indicators()

        def _update_rate_indicators(self):
                for n, photon_count, lost_count, timestamp in self.pipeline.stats():
                        last_photon_count, last_lost_count, last_timestamp = self.last_stats[n]
                        if last_timestamp != timestamp:
                                photon_rate = (photon_count - last_photon_count) / (timestamp - last_timestamp)
                                loss_rate = (lost_count - last_lost_count) / (timestamp - last_timestamp)

				markup = "<span color='darkgreen'size='xx-large'>%d</span> <span size='large'>photons/second</span>" % photon_rate
				self.inputs[n][0].set_markup(markup)
				markup = "<span color='darkred'size='xx-large'>%d</span> <span size='large'>loss events/second</span>" % loss_rate
				self.inputs[n][1].set_markup(markup)
                        self.last_stats[n] = (photon_count, lost_count, timestamp)


        def _update_total_indicators(self):
                for n, photon_count, lost_count, timestamp in self.pipeline.stats():
			markup = "<span color='darkgreen'size='xx-large'>%1.3e</span> <span size='large'>photons</span>" % photon_count
			self.inputs[n][0].set_markup(markup)
			markup = "<span color='darkred'size='xx-large'>%d</span> <span size='large'>loss events</span>" % lost_count
			self.inputs[n][1].set_markup(markup)
				

        def start_pipeline(self):
                if self.pipeline:
                        raise "Tried to start a capture pipeline while one is already running"

                file = None
                if self.builder.get_object('file_output_enabled').props.active:
                        file = self.builder.get_object('output_file').props.text

                bin_time = self.builder.get_object('bin_time').props.value
                self.pipeline = CapturePipeline(output_file=file, bin_time=bin_time*1e-3)
                #self.pipeline = TestPipeline(100)
                self.pipeline.start()

                # Start update loop for plot
                self.last_stats = defaultdict(lambda: (0, 0, 0))
                def update_cb():
                        try:
                                self.update_plot()
                                self.update_indicators()
                        except:
                                pass
                        return self.pipeline != None
                gobject.timeout_add(int(1000.0/self.update_rate), update_cb)

                # Sensitize outputs
                for c in self.outputs:
                        c.pipeline_started()

        def stop_pipeline(self):
                for c in self.outputs:
                        c.pipeline_stopped()
                self.stop_readout()
                self.pipeline.stop()
                self.pipeline = None
                # As a precaution to prevent accidental overwriting of acquired data
                self.builder.get_object('file_output_enabled').props.active = False

        def pipeline_running_toggled_cb(self, action):
                state = action.props.active
                for o in [ 'file_output_enabled', 'output_file', 'select_output_file' ]:
                        self.builder.get_object(o).props.sensitive = not state
                for o in [ 'readout_running', 'stop_outputs', 'start_outputs' ]:
                        self.builder.get_object(o).props.sensitive = state

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


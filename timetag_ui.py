#!/usr/bin/python

import logging
from collections import defaultdict
import time
import os

import gobject, gtk
import matplotlib
from matplotlib.figure import Figure
from matplotlib.backends.backend_gtk import FigureCanvasGTK as FigureCanvas
#from matplotlib.backends.backend_gtkagg import FigureCanvasGTKAgg as FigureCanvas

from capture_pipeline import CapturePipeline, TestPipeline

logging.basicConfig(level=logging.DEBUG)

PULSESEQ_FREQ = 30e6

resource_prefix = '/usr/share/timetag'
default_configs = [ os.path.expanduser('~/.timetag.cfg'),
		    os.path.join(resource_prefix, 'default.cfg') ]

class OutputChannel(object):
        sensitive_widgets = [ 'initial_state', 'running', 'offset_time_spin', 'high_time_spin', 'low_time_spin', ]

        def __init__(self, main_window, output_n):
		self.builder = gtk.Builder()
                self.builder.add_from_file(os.path.join(resource_prefix, 'output_channel.glade'))
                self.builder.connect_signals(self)
                self.widget = self.builder.get_object('output_channel')

                self.icon = gtk.image_new_from_stock('Stop', gtk.ICON_SIZE_BUTTON)
                header = gtk.HBox()
                header.pack_start(self.icon)
		label = gtk.Label()
                header.pack_start(label)
                header.show_all()
                self.notebook_label = header
		self.label = label

		self.main_window = main_window
                self.output_n = output_n
		self.name = 'Channel %d' % output_n
                self._set_sensitivity(False)
	
	@property
	def name(self):
		return self.builder.get_object('name').props.text
	@name.setter
	def name(self, value):
		self.builder.get_object('name').props.text = value
		self.label.props.label = value

	@property
	def running(self):
		return self.builder.get_object('running').props.active
	@running.setter
	def running(self, value):
		self.builder.get_object('running').props.active = value

	@property
	def offset_time(self):
		return self.builder.get_object('offset_time').props.value
	@offset_time.setter
	def offset_time(self, value):
		self.builder.get_object('offset_time').props.value = value
		self._update_offset_time()

	@property
	def initial_state(self):
		return self.builder.get_object('initial_state').props.active
	@initial_state.setter
	def initial_state(self, value):
		self.builder.get_object('initial_state').props.active = value
		self._update_initial_state()

	@property
	def high_time(self):
		return self.builder.get_object('high_time').props.value
	@high_time.setter
	def high_time(self, value):
		self.builder.get_object('high_time').props.value = value
		self._update_high_time()

	@property
	def low_time(self):
		return self.builder.get_object('low_time').props.value
	@low_time.setter
	def low_time(self, value):
		self.builder.get_object('low_time').props.value = value
		self._update_low_time()

        def pipeline_started(self):
                self._set_sensitivity(True)
		self._update_running_state()
		self._update_initial_state()
		self._update_offset_time()
		self._update_low_time()
		self._update_high_time()

        def pipeline_stopped(self):
                self._set_sensitivity(False)

        def _set_sensitivity(self, state):
                for w in self.__class__.sensitive_widgets:
                        self.builder.get_object(w).props.sensitive = state

        @property
        def tagger(self):
                if not self.main_window.pipeline: return None
                return self.main_window.pipeline.tagger

	def _update_running_state(self):
		action = self.builder.get_object('running')
                if self.running:
                        action.props.label = 'Running'
                        self.icon.set_from_stock('Play', gtk.ICON_SIZE_MENU)
                else:
                        action.props.label = 'Stopped'
                        self.icon.set_from_stock('Stop', gtk.ICON_SIZE_MENU)

                if not self.tagger: return
                if self.running:
                        self.tagger.start_outputs([self.output_n])
                else:
                        self.tagger.stop_outputs([self.output_n])
                        self.tagger.set_initial_state(self.output_n, self.initial_state)

	def _update_offset_time(self):
                if not self.tagger: return
		time = self.builder.get_object('offset_time').props.value
                count = time * PULSESEQ_FREQ / 1e3
                self.tagger.set_initial_count(self.output_n, count)

	def _update_high_time(self):
                if not self.tagger: return
		time = self.builder.get_object('high_time').props.value
                count = time * PULSESEQ_FREQ / 1e3
                self.tagger.set_high_count(self.output_n, count)

	def _update_low_time(self):
                if not self.tagger: return
		time = self.builder.get_object('low_time').props.value
                count = time * PULSESEQ_FREQ / 1e3
                self.tagger.set_low_count(self.output_n, count)

	def _update_initial_state(self):
		action = self.builder.get_object('initial_state')
		state = action.props.active
                if state:
                        action.props.label = "High"
                else:
                        action.props.label = "Low"

                if not self.tagger: return
                self.tagger.set_initial_state(self.output_n, state)

        def offset_time_value_changed_cb(self, adj):
		self._update_offset_time()
        def high_time_value_changed_cb(self, adj):
		self._update_high_time()
        def low_time_value_changed_cb(self, adj):
		self._update_low_time()
        def initial_state_toggled_cb(self, action):
                self._update_initial_state()
        def running_toggled_cb(self, action):
		self._update_running_state()
	def name_changed_cb(self, editable):
		self.name = editable.props.text


class NumericalIndicators(object):
        def __init__(self, n_inputs, main_win):
                self.main_win = main_win
                self.rate_mode = True
                self.last_stats = defaultdict(lambda: (0, 0, 0))

		self.inputs = []
		table = gtk.Table(n_inputs, 3)
		for c in range(n_inputs):
			label, photons, lost = gtk.Label(), gtk.Label(), gtk.Label()
			label.set_markup('<span size="large">Channel %d</span>' % c)
			table.attach(label, 0,1, c,c+1)
			table.attach(photons, 1,2, c,c+1)
			table.attach(lost, 2,3, c,c+1)
			self.inputs.append((photons, lost))

                self.widget = table

        def update(self):
                if self.rate_mode:
                        self._update_rate_indicators()
                else:
                        self._update_total_indicators()

        def _update_rate_indicators(self):
                for n, photon_count, lost_count, timestamp in self.main_win.pipeline.stats():
                        last_photon_count, last_lost_count, last_timestamp = self.last_stats[n]
                        if last_timestamp != timestamp:
                                photon_rate = (photon_count - last_photon_count) / (timestamp - last_timestamp)
                                loss_rate = (lost_count - last_lost_count) / (timestamp - last_timestamp)

				markup = "<span color='darkgreen' size='xx-large'>%d</span> <span size='large'>photons/second</span>" % photon_rate
				self.inputs[n][0].set_markup(markup)
				markup = "<span color='darkred' size='xx-large'>%d</span> <span size='large'>loss events/second</span>" % loss_rate
				self.inputs[n][1].set_markup(markup)
                        self.last_stats[n] = (photon_count, lost_count, timestamp)


        def _update_total_indicators(self):
                for n, photon_count, lost_count, timestamp in self.main_win.pipeline.stats():
			markup = "<span color='darkgreen' size='xx-large'>%1.3e</span> <span size='large'>photons</span>" % photon_count
			self.inputs[n][0].set_markup(markup)
			markup = "<span color='darkred' size='xx-large'>%d</span> <span size='large'>loss events</span>" % lost_count
			self.inputs[n][1].set_markup(markup)


class Plot(object):
        def __init__(self, main_win):
                self.scroll = False
                self.width = 1
                self.y_bounds = None

                self.main_win = main_win
                self.sync_timestamp = 0
                self.sync_walltime = 0
                self.figure = Figure()
                self.axes = self.figure.add_subplot(111)
                self.axes.get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter(useOffset=False))
                self.lines = {}
                self.canvas = FigureCanvas(self.figure)

        @property
        def pipeline(self):
                return self.main_win.pipeline

        def update(self):
                if not self.pipeline:
                        return False

                for n,times,counts in self.pipeline.bins():
                        if not self.lines.has_key(n):
                                self.lines[n], = self.axes.plot(times, counts)#, animated=True)
                        else:
                                self.lines[n].set_data(times, counts)

                self.axes.relim()

		# Scale X axis:
                def calc_x_bounds():
                        xmax = self.sync_timestamp
                        if self.scroll:
                                xmax += time.time() - self.sync_walltime
                        xmin = xmax - self.width
                        return xmin, xmax

                xmin, xmax = calc_x_bounds()
                if not xmin < self.pipeline.latest_timestamp < xmax:
                        self.sync_walltime = time.time()
                        self.sync_timestamp = self.pipeline.latest_timestamp
                        xmin, xmax = calc_x_bounds()
                        
		self.axes.set_xlim(xmin, xmax)

		# Scale Y axis:
		ymin,ymax = None,None
		if self.y_bounds:
                        ymin, ymax = self.y_bounds
                else:
			self.axes.autoscale_view(scalex=False, scaley=True, tight=False)
			_,ymax = self.axes.get_ylim()
			ymax *= 1.1
			ymin = 0
                self.axes.set_ylim(ymin, ymax)

                self.figure.canvas.draw()


class MainWindow(object):
        def __init__(self, n_inputs=4):
                self.update_rate = 30 # in Hertz
                self.pipeline = None

                self.builder = gtk.Builder()
                self.builder.add_from_file(os.path.join(resource_prefix, 'timetag_ui.glade'))
                self.builder.connect_signals(self)

                def quit(unused):
                        if self.pipeline:
                                self.pipeline.stop()
                        gtk.main_quit()
                self.win = self.builder.get_object('main_window')
                self.win.connect('destroy', quit)
                
                self.indicators = NumericalIndicators(n_inputs, self)
                self.builder.get_object('channel_stats').pack_start(self.indicators.widget)

                self.outputs = []
                notebook = self.builder.get_object('output_channels')
                for c in range(4):
                        chan = OutputChannel(self, c)
                        notebook.append_page(chan.widget, chan.notebook_label)
                        self.outputs.append(chan)

                self.plot = Plot(self)
                self.builder.get_object('plot_container').pack_start(self.plot.canvas)
		for f in default_configs:
			if os.path.isfile(f):
				self.load_config(f)
				break

                self.win.show_all()

        def select_output_file_activate_cb(self, action):
                fc = gtk.FileChooserDialog('Select output file', self.win, gtk.FILE_CHOOSER_ACTION_SAVE,
                        (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL,  gtk.STOCK_OK, gtk.RESPONSE_OK))
                fc.props.do_overwrite_confirmation = True
                res = fc.run()
                fc.hide()
                if res == gtk.RESPONSE_OK:
                        self.builder.get_object('output_file').props.text = fc.get_filename()

        def start_pipeline(self):
                if self.pipeline:
                        raise "Tried to start a capture pipeline while one is already running"

                file = None
                if self.builder.get_object('file_output_enabled').props.active:
                        file = self.builder.get_object('output_file').props.text

                self.pipeline = CapturePipeline(output_file=file, bin_time=self.bin_time, npts=self.n_points)
                #self.pipeline = TestPipeline(100)
                self.pipeline.start()

                # Start update loop for plot
                def update_cb():
                        try:
                                self.plot.update()
                                self.indicators.update()
                        except AttributeError as e:
                                # Ignore exceptions if pipeline is shut down
                                if not self.pipeline: return False
                                raise e
                        return True

                gobject.timeout_add(int(1000.0/self.update_rate), update_cb)

                # Sensitize outputs
                for c in self.outputs:
                        c.pipeline_started()

        @property
        def bin_time(self):
                return self.builder.get_object('bin_time').props.value / 1000.0

        @property
        def plot_width(self):
		return self.builder.get_object('x_width').props.value
        
        @property
        def n_points(self):
                """ The required number of points to fill the entire
                width of the plot at the given bin_time """
                return self.plot_width / self.bin_time

	def x_width_value_changed_cb(self, *args):
		if not self.pipeline: return
		self.pipeline.resize_buffer(self.n_points)
                self.plot.width = self.plot_width

        def stop_pipeline(self):
                for c in self.outputs:
                        c.pipeline_stopped()
                self.stop_readout()
                self.pipeline.stop()
                self.pipeline = None
                # As a precaution to prevent accidental overwriting of acquired data
                self.builder.get_object('file_output_enabled').props.active = False

        def pipeline_running_toggled_cb(self, action):
		get_object = self.builder.get_object
                state = action.props.active
                for o in [ 'file_output_enabled', 'output_file', 'select_output_file', 'bin_time_spin' ]:
                        get_object(o).props.sensitive = not state
                for o in [ 'readout_running', 'stop_outputs', 'start_outputs' ]:
                        get_object(o).props.sensitive = state

                if state:
                        self.start_pipeline()
                else:
			get_object('readout_running').set_active(False)
                        self.stop_pipeline()

        def start_readout(self):
                self.pipeline.tagger.start_capture()
                self.plot.scroll = True

        def stop_readout(self):
                self.pipeline.tagger.stop_capture()
                self.plot.scroll = False

        def y_auto_toggled_cb(self, action):
                state = action.props.active
                for o in [ 'y_upper_spin', 'y_lower_spin' ]:
                        self.builder.get_object(o).props.sensitive = not state

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

        def indicator_mode_changed_cb(self, widget):
                self.indicators.rate_mode = bool(self.builder.get_object('show_rates').props.active)

	def load_config_activate_cb(self, action):
                fc = gtk.FileChooserDialog('Select configuration file', self.win, gtk.FILE_CHOOSER_ACTION_OPEN,
                        (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL,  gtk.STOCK_OK, gtk.RESPONSE_OK))
                res = fc.run()
                fc.hide()
                if res == gtk.RESPONSE_OK:
                        self.load_config(fc.get_filename())

	def save_config_activate_cb(self, action):
                fc = gtk.FileChooserDialog('Select configuration file', self.win, gtk.FILE_CHOOSER_ACTION_SAVE,
                        (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL,  gtk.STOCK_OK, gtk.RESPONSE_OK))
                fc.props.do_overwrite_confirmation = True
                res = fc.run()
                fc.hide()
                if res == gtk.RESPONSE_OK:
                        self.save_config(fc.get_filename())
		
	def load_config(self, file):
		from ConfigParser import ConfigParser
		get_object = self.builder.get_object
		config = ConfigParser()
		config.read(file)

		for sect in config.sections():
			if sect.startswith("output-"):
				output_n = int(sect[len('output-'):])
				chan = self.outputs[output_n]
				chan.name = config.get(sect, 'name')
				chan.initial_state = config.getboolean(sect, 'initial-state')
				chan.offset_time = config.getfloat(sect, 'offset-time')
				chan.high_time = config.getfloat(sect, 'high-time')
				chan.low_time = config.getfloat(sect, 'low-time')

		get_object('bin_time').props.value = config.getfloat('acquire', 'bin_time')
		get_object('x_width').props.value = config.getfloat('acquire', 'plot_width')

	def save_config(self, file):
		from ConfigParser import ConfigParser
		get_object = self.builder.get_object
		config = ConfigParser()

		for n,o in enumerate(self.outputs):
			sect = 'output-%d' % n
			config.add_section(sect)
			config.set(sect, 'name', o.name)
			config.set(sect, 'initial-state', o.initial_state)
			config.set(sect, 'offset-time', o.offset_time)
			config.set(sect, 'high-time', o.high_time)
			config.set(sect, 'low-time', o.low_time)

		config.add_section('acquire')
		config.set('acquire', 'bin_time', get_object('bin_time').props.value)
		config.set('acquire', 'plot_width', get_object('x_width').props.value)
		config.write(open(file,'w'))

if __name__ == '__main__':
        gtk.gdk.threads_init()
        win = MainWindow()
        gtk.main()


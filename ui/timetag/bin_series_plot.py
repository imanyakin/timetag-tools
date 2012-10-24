import logging
import time
import pkgutil

import gobject, gtk
import matplotlib
from matplotlib.figure import Figure
from matplotlib.backends.backend_gtk import FigureCanvasGTK
from matplotlib.backends.backend_gtkagg import FigureCanvasGTKAgg
from matplotlib.backends.backend_gtkcairo import FigureCanvasGTKCairo

from timetag.binner import BufferBinner
from timetag.managed_binner import ManagedBinner
from timetag import config

def fix_color(c):
        c = gtk.gdk.color_parse(c)
        return (c.red_float, c.green_float, c.blue_float)

class BinSeriesPlot(ManagedBinner):
        FigureCanvas = FigureCanvasGTK

        def __init__(self, pipeline):
                self.pipeline = pipeline
                self.builder = gtk.Builder()
                src = pkgutil.get_data('timetag', 'bin_series.glade')
                self.builder.add_from_string(src)
                self.builder.connect_signals(self)
                self.win = self.builder.get_object('bin_series_window')
                self.win.connect('destroy', self.destroy_cb)

                rc = config.load_rc()
                self.colors = map(lambda chan: fix_color(chan.color), rc['strobe-channels'])
                self.plot_update_rate = 12 # in Hertz
                self.y_bounds = None

                self.running = True
                self._setup_plot()
                self.win.show_all()
		ManagedBinner.__init__(self, self.pipeline, 'bin-series')

	def on_started(self):
		self._start_fps_display()
                gobject.timeout_add(int(1000.0/self.plot_update_rate), self._update_plot)

        def _start_fps_display(self):
                self.fps_interval = 5 # seconds
                self.frame_cnt = 0
                def display_fps():
                        if self.frame_cnt > 0:
			    fps = self.frame_cnt / self.fps_interval
			    self.frame_cnt = 0
			    logging.debug("Plot: %2.1f FPS" % fps)
                        return self.is_running()

                gobject.timeout_add_seconds(self.fps_interval, display_fps)

        def destroy_cb(self, a):
                self.stop_binner()
                
        def _setup_plot(self):
                self.last_timestamp = 0
                self.sync_timestamp = 0
                self.sync_walltime = 0
                self.figure = Figure()
                self.axes = self.figure.add_subplot(111)
                self.axes.get_xaxis().set_major_formatter(
                                matplotlib.ticker.ScalarFormatter(useOffset=False))
                self.axes.set_xlabel('Time (s)')
                self.axes.set_ylabel('Counts per bin')
                self.lines = {}
                canvas = self.__class__.FigureCanvas(self.figure)
                self.builder.get_object('plot_container').pack_start(canvas)

        def create_binner(self):
                binner = BufferBinner(self.bin_time, self.pipeline.clockrate)
		binner.resize_buffer(self.n_points)
		return binner

        def _update_plot(self):
                max_counts = 1
                clockrate = self.pipeline.clockrate
		binner = self.get_binner()
		if binner is None: return False
                for n,channel in enumerate(binner.channels):
                        bins = channel.counts.get()
                        if len(bins) == 0:
                                continue
                        max_counts = max(max_counts, max(bins['counts']))
                        # FIXME
                        #if not self.main_win.strobe_config[n].enabled: continue
                        if not self.lines.has_key(n):
                                self.lines[n], = self.axes.plot(bins['time'],
                                                                bins['counts'],
                                                                color=self.colors[n])
                        else:
                                self.lines[n].set_data(bins['time'], bins['counts'])

                self.axes.relim()

                # Scale X axis:
                def calc_x_bounds():
                        xmax = self.sync_timestamp / clockrate
                        xmax += time.time() - self.sync_walltime
                        xmin = xmax - self.plot_width
                        return xmin, xmax

                xmin, xmax = calc_x_bounds()
                if not xmin < binner.latest_timestamp / clockrate < xmax:
                        self.sync_walltime = time.time()
                        self.sync_timestamp = binner.latest_timestamp
                        xmin, xmax = calc_x_bounds()

                self.axes.set_xlim(xmin, xmax)

                # Scale Y axis:
                ymin,ymax = 0, 1.1*max_counts
                if self.y_bounds:
                        ymin, ymax = self.y_bounds
                self.axes.set_ylim(ymin, ymax)

                self.figure.canvas.draw()
                self.frame_cnt += 1
		return self.is_running()

        @property
        def plot_width(self):
                return self.builder.get_object('x_width').props.value

        @property
        def n_points(self):
                """ The required number of points to fill the entire
                width of the plot at the given bin_time """
                return self.plot_width / self.bin_time

        @property
        def bin_time(self):
                return 1e-3 * self.builder.get_object('bin_time').props.value

        def x_width_value_changed_cb(self, *args):
		self.restart_binner()

        def y_bounds_changed_cb(self, *args):
                get_object = self.builder.get_object

                auto = get_object('y_auto').props.active
                for o in [ 'y_upper_spin', 'y_lower_spin' ]:
                        get_object(o).props.sensitive = not auto

                if auto:
                        self.y_bounds = None
                else:
                        self.y_bounds = (get_object('y_lower').props.value,
                                         get_object('y_upper').props.value)

        def bin_time_changed_cb(self, *args):
                self.restart_binner()

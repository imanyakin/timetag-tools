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

# FIXME
def_colors = ['#A80505', '#006619', '#0142D5', '#922FFF']

def fix_color(c):
        c = gtk.gdk.color_parse(c)
        return (c.red_float, c.green_float, c.blue_float)

class BinSeriesPlot(object):
        FigureCanvas = FigureCanvasGTK

        def __init__(self, pipeline):
                self.builder = gtk.Builder()
                src = pkgutil.get_data('timetag', 'bin_series.glade')
                self.builder.add_from_string(src)
                self.builder.connect_signals(self)
                self.win = self.builder.get_object('bin_series_window')
                self.win.connect('destroy', self.destroy_cb)
                self.pipeline = pipeline

                self.colors = map(fix_color, def_colors)
                self.plot_update_rate = 12 # in Hertz
                self.width = 10 # seconds
                self.y_bounds = None

                self.running = True
                self.binner = None
                self._restart_binner()
                self._setup_plot()
                self.start_fps_display()
                self.win.show_all()

        def destroy_cb(self, a):
                self.running = False
                self._stop_binner()

        def start_fps_display(self):
                self.fps_interval = 5 # seconds
                self.frame_cnt = 0
                def display_fps():
                        if not self.frame_cnt > 0: return True
                        fps = self.frame_cnt / self.fps_interval
                        self.frame_cnt = 0
                        logging.debug("Plot: %2.1f FPS" % fps)
                        return self.running

                gobject.timeout_add_seconds(self.fps_interval, display_fps)
                
        def _setup_plot(self):
                self.last_timestamp = 0
                self.sync_timestamp = 0
                self.sync_walltime = 0
                self.builder.get_object('x_width').props.value = 10 # HACK: set default
                self.figure = Figure()
                self.axes = self.figure.add_subplot(111)
                self.axes.get_xaxis().set_major_formatter(
                                matplotlib.ticker.ScalarFormatter(useOffset=False))
                self.axes.set_xlabel('Time (s)')
                self.axes.set_ylabel('Counts per bin')
                self.lines = {}
                canvas = self.__class__.FigureCanvas(self.figure)
                self.builder.get_object('plot_container').pack_start(canvas)

                # Start update loop
                def update_plot():
                        try:
                                self._update()
                        except AttributeError as e:
                                # Ignore exceptions in case pipeline is shut down
                                raise e
                        return self.running

                gobject.timeout_add(int(1000.0/self.plot_update_rate), update_plot)

        def _stop_binner(self):
                if self.binner is not None:
                        self.pipeline.remove_output(self.output_id)
                        self.binner.stop()

        def _restart_binner(self):
                self._stop_binner()
                self.binner = BufferBinner(self.bin_time, self.pipeline.clockrate)
                self.output_id = str(id(self))
                self.pipeline.add_output(self.output_id, self.binner.get_data_fd())

        def _update(self):
                max_counts = 1
                clockrate = self.pipeline.clockrate
                for n,channel in enumerate(self.binner.channels):
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
                        xmin = xmax - self.width
                        return xmin, xmax

                xmin, xmax = calc_x_bounds()
                if not xmin < self.binner.latest_timestamp / clockrate < xmax:
                        self.sync_walltime = time.time()
                        self.sync_timestamp = self.binner.latest_timestamp
                        xmin, xmax = calc_x_bounds()

                self.axes.set_xlim(xmin, xmax)

                # Scale Y axis:
                ymin,ymax = 0, 1.1*max_counts
                if self.y_bounds:
                        ymin, ymax = self.y_bounds
                self.axes.set_ylim(ymin, ymax)

                self.figure.canvas.draw()
                self.frame_cnt += 1

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
                if not self.pipeline: return
                self.binner.resize_buffer(self.n_points)
                self.width = self.plot_width

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
                self._restart_binner()

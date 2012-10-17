import time
import pkgutil

import gobject, gtk
import matplotlib
from matplotlib.figure import Figure
from matplotlib.backends.backend_gtk import FigureCanvasGTK
from matplotlib.backends.backend_gtkagg import FigureCanvasGTKAgg
from matplotlib.backends.backend_gtkcairo import FigureCanvasGTKCairo

class BinSeriesPlot(object):
        FigureCanvas = FigureCanvasGTK

        def __init__(self, main_win):
                def fix_color(c):
                        c = gtk.gdk.color_parse(c.color)
                        return (c.red_float, c.green_float, c.blue_float)
                self.colors = [ fix_color(c) for c in main_win.strobe_config ]

                self.plot_update_rate = 12 # in Hertz
                self.builder = gtk.Builder()
                src = pkgutil.get_data('timetag', 'bin_series.glade')
                self.builder.add_from_string(src)
                self.builder.connect_signals(self)
                self.win = self.builder.get_object('bin_series_window')
                def closed(unused):
                        main_win.bin_series_win_closed()
                self.win.connect('destroy', closed)

                self.scroll = False
                self.width = 10 # seconds
                self.y_bounds = None

                self.main_win = main_win
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
                self.win.show_all()

                self.fps_interval = 5 # seconds
                self.frame_cnt = 0
                def display_fps():
                        if not self.frame_cnt > 0: return True
                        fps = self.frame_cnt / self.fps_interval
                        self.frame_cnt = 0
                        logging.debug("Plot: %2.1f FPS" % fps)
                        return True
                gobject.timeout_add_seconds(self.fps_interval, display_fps)

                # Start update loop
                def update_plot():
                        if not self.pipeline or not self.running:
                                return False
                        try:
                                self.update()
                        except AttributeError as e:
                                # Ignore exceptions in case pipeline is shut down
                                raise e
                        return True

                self.running = True
                gobject.timeout_add(int(1000.0/self.plot_update_rate), update_plot)

        def close(self):
                self.running = False
                self.win.hide()

        @property
        def pipeline(self):
                return self.main_win.pipeline

        def update(self):
                if not self.pipeline:
                        return False

                max_counts = 0
                for n,times,counts in self.pipeline.bins():
                        max_counts = max(max_counts, max(counts))
                        if not self.main_win.strobe_config[n].enabled: continue
                        if not self.lines.has_key(n):
                                self.lines[n], = self.axes.plot(times, counts, color=self.colors[n])
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
                return self.plot_width / self.main_win.bin_time

        def x_width_value_changed_cb(self, *args):
                if not self.pipeline: return
                self.pipeline.resize_buffer(self.n_points)
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

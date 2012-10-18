import time
import pkgutil

import gobject, gtk
import matplotlib
from matplotlib.figure import Figure
from matplotlib.backends.backend_gtk import FigureCanvasGTK
from matplotlib.backends.backend_gtkagg import FigureCanvasGTKAgg
from matplotlib.backends.backend_gtkcairo import FigureCanvasGTKCairo
from collections import defaultdict

from timetag.binner import HistBinner

# FIXME
def_colors = ['#A80505', '#006619', '#0142D5', '#922FFF']

def fix_color(c):
        c = gtk.gdk.color_parse(c)
        return (c.red_float, c.green_float, c.blue_float)

class HistPlot(object):
        FigureCanvas = FigureCanvasGTK

        def __init__(self, main_win):
                self.bin_time = 1e-2
                self.colors = map(fix_color, def_colors)
                self.builder = gtk.Builder()
                src = pkgutil.get_data('timetag', 'hist.glade')
                self.builder.add_from_string(src)
                self.builder.connect_signals(self)
                self.win = self.builder.get_object('hist_window')
                self.win.connect('destroy', self.destroy_cb)
                self.pipeline = main_win.pipeline
                self.update_rate = 0.3 # Hz

                self.binner = None
                self._restart_binner()

                self.figure = Figure()
                self.axes = []
                for i in range(4):
                        axes = self.figure.add_subplot(140+i)
                        axes.get_xaxis().set_major_formatter(
                                        matplotlib.ticker.ScalarFormatter(useOffset=False))
                        axes.set_xlabel('Counts per bin')
                        self.axes.append(axes)

                canvas = self.__class__.FigureCanvas(self.figure)
                self.builder.get_object('plot_container').pack_start(canvas)
                self.builder.get_object('bin_width').value = 0.02 # HACK: set default
                self.win.show_all()

                def update_plot():
                        try:
                                self.update()
                        except AttributeError as e:
                                # Ignore exceptions in case pipeline is shut down
                                raise e
                        return self.running

                self.running = True
                gobject.timeout_add(int(1000.0 / self.update_rate), update_plot)

        def destroy_cb(self, a):
                self.running = False
                self._stop_binner()

        def _stop_binner(self):
                if self.binner is not None:
                        self.pipeline.remove_output(self.output_id)
                        self.binner.stop()

        def _restart_binner(self):
                self._stop_binner()
                self.binner = HistBinner(self.bin_time, self.pipeline.clockrate)
                self.output_id = str(id(self))
                self.pipeline.add_output(self.output_id, self.binner.get_data_fd())

        def update(self):
                hist_width = self.binner.hist_width
                for c,hist in enumerate(self.binner.channels):
                        if len(hist) == 0: continue
                        self.axes[c].cla()
                        self.axes[c].bar(hist.keys(), hist.values(), hist_width,
                                         color=self.colors[c])
                        self.axes[c].relim()

                self.figure.canvas.draw()

        def bin_width_changed_cb(self, adj):
                self.binner.hist_width = adj.get_value()


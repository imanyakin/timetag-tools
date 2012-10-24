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
from timetag.managed_binner import ManagedBinner
from timetag import config

def fix_color(c):
        c = gtk.gdk.color_parse(c)
        return (c.red_float, c.green_float, c.blue_float)

class HistPlot(ManagedBinner):
        FigureCanvas = FigureCanvasGTK

        def __init__(self, pipeline):
                self.builder = gtk.Builder()
                src = pkgutil.get_data('timetag', 'hist.glade')
                self.builder.add_from_string(src)
                self.builder.connect_signals(self)
                self.win = self.builder.get_object('hist_window')
                self.win.connect('destroy', self.destroy_cb)
                self.pipeline = pipeline
                self.update_rate = 0.3 # Hz

                rc = config.load_rc()
                self.colors = map(lambda chan: fix_color(chan.color), rc['strobe-channels'])
                self.figure = Figure()
                self.axes = []
                for i in range(4):
                        axes = self.figure.add_subplot(140+i)
                        axes.get_xaxis().set_major_formatter(
                                        matplotlib.ticker.ScalarFormatter(useOffset=False))
                        self.axes.append(axes)

                canvas = self.__class__.FigureCanvas(self.figure)
                self.builder.get_object('plot_container').pack_start(canvas)
                self.win.show_all()
                ManagedBinner.__init__(self, self.pipeline, 'hist-plot')

        def create_binner(self):
                return HistBinner(self.bin_time, self.pipeline.clockrate)

        def on_started(self):
                gobject.timeout_add(int(1000.0 / self.update_rate), self._update_plot)

        def destroy_cb(self, a):
                self.stop_binner()

        def _update_plot(self):
                if self.get_binner() is None: return False
                hist_width = self.get_binner().hist_width
                for c,hist in enumerate(self.get_binner().channels):
                        if len(hist) == 0: continue
                        self.axes[c].cla()
                        self.axes[c].bar(hist.keys(), hist.values(), hist_width,
                                         color=self.colors[c])
                        self.axes[c].relim()
                        self.axes[c].set_xlabel('Counts per bin')

                self.figure.canvas.draw()
                return True

        @property
        def bin_time(self):
                return self.builder.get_object('bin_width').props.value
        
        def bin_width_changed_cb(self, adj):
                self.restart_binner()


import time
import pkgutil

import gobject, gtk
import matplotlib
from matplotlib.figure import Figure
from matplotlib.backends.backend_gtk import FigureCanvasGTK
from matplotlib.backends.backend_gtkagg import FigureCanvasGTKAgg
from matplotlib.backends.backend_gtkcairo import FigureCanvasGTKCairo

class HistPlot(object):
        FigureCanvas = FigureCanvasGTKCairo
        def __init__(self, main_win):
                def fix_color(c):
                        c = gtk.gdk.color_parse(c.color)
                        return (c.red_float, c.green_float, c.blue_float)
                self.colors = [ fix_color(c) for c in main_win.strobe_config ]

                self.builder = gtk.Builder()
                src = pkgutil.get_data('timetag', 'hist.glade')
                self.builder.add_from_string(src)
                self.builder.connect_signals(self)
                self.win = self.builder.get_object('hist_window')
                def closed(unused):
                        main_win.hist_win_closed()
                self.win.connect('destroy', closed)

                self.main_win = main_win
                self.pipeline = main_win.pipeline
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
                self.builder.get_object('bin_width').value = 10 # HACK: set default
                self.win.show_all()

                def update_plot():
                        if not self.running:
                                return False
                        try:
                                self.update()
                        except AttributeError as e:
                                # Ignore exceptions in case pipeline is shut down
                                raise e
                        return True

                self.running = True
                gobject.timeout_add(int(1000.0/3), update_plot)

        def close(self):
                self.running = False
                self.win.hide()

        def update(self):
                hist_width = self.pipeline.hist_width
                for c,chan in self.pipeline.channels.items():
                        h = chan.hist
                        if len(h) == 0: continue
                        self.axes[c].cla()
                        self.axes[c].bar(h.keys(), h.values(), hist_width, alpha=0.5, color=self.colors[c])
                        self.axes[c].relim()

                self.figure.canvas.draw()

        def bin_width_value_changed_cb(self, adj):
                self.pipeline.hist_width = adj.get_value()


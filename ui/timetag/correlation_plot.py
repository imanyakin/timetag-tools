import logging
import time
import pkgutil

import gobject, gtk
import matplotlib
from matplotlib.figure import Figure
from matplotlib.backends.backend_gtkagg import FigureCanvasGTKAgg
from matplotlib.backends.backend_gtkcairo import FigureCanvasGTKCairo

from timetag.binner import BufferBinner
from timetag.managed_binner import ManagedBinner
from timetag import config

import numpy as np 
from dls.numerics import crosscorrelation as xcs
from dls.numerics import autocorrelation as acs

import timeit
def fix_color(c):
        c = gtk.gdk.color_parse(c)
        return (c.red_float, c.green_float, c.blue_float)

class CorrelationPlot(ManagedBinner):
    FigureCanvas = FigureCanvasGTKAgg

    def __init__(self, pipeline):
        self.pipeline = pipeline
        self.builder = gtk.Builder()
        src = pkgutil.get_data('timetag', 'correlate.glade')
        self.builder.add_from_string(src)
        self.builder.connect_signals(self)
        self.win = self.builder.get_object('correlate_window')
        self.win.connect('destroy', self.destroy_cb)

        rc = config.load_rc()
        self.colors = map(lambda chan: fix_color(chan.color), rc['strobe-channels'])
        self.plot_update_rate = 1.0 # in Hertz
        self.y_bounds = None

        self.running = True
        self._setup_plot()
        self.win.show_all()

        ManagedBinner.__init__(self, self.pipeline, 'correlator')
        #set cross-correlation channels
        self.xcs0 = 1
        self.xcs1 = 1

    def on_started(self):
        fps = 2.0
        function_call_interval = 1000.0/fps #ms
        # while True:
            # self._update_plot()
            # time.sleep(0.5)
        gobject.timeout_add(int(function_call_interval), self._update_plot,priority=gobject.PRIORITY_HIGH_IDLE+20)
       
    def destroy_cb(self, a):
            self.stop_binner()
            gtk.main_quit()
            
    def _setup_plot(self):
            self.last_timestamp = 0
            self.sync_timestamp = 0
            self.sync_walltime = 0
            self.figure = Figure()
            self.axes = self.figure.add_subplot(111)
            self.axes.get_xaxis().set_major_formatter(
                            matplotlib.ticker.ScalarFormatter(useOffset=False))
            self.axes.set_xlabel('Time (s)')
            self.axes.set_ylabel('Autocorrelation')
            self.lines = {}
            canvas = self.__class__.FigureCanvas(self.figure)
            self.builder.get_object('plot_container').pack_start(canvas)

    def create_binner(self):
        binner = BufferBinner(self.bin_time, self.pipeline.clockrate)
        binner.resize_buffer(self.n_points)
        return binner

    def _update_plot(self):
        t0 = time.time()
        max_counts = 1
        clockrate = self.pipeline.clockrate
        binner = self.get_binner()
        if binner is None:
            return False

        def calc_x_bounds():
                xmax = self.sync_timestamp / clockrate
                xmax += time.time() - self.sync_walltime
                xmin = xmax - 1.0
                return xmin, xmax

        xmin, xmax = calc_x_bounds()
        if not xmin < binner.latest_timestamp / clockrate < xmax:
                self.sync_walltime = time.time()
                self.sync_timestamp = binner.latest_timestamp
                xmin, xmax = calc_x_bounds()

        max_acf = 2.0
        #
        tmax = -1
        
        ch1 = binner.channels[self.xcs0-1]
        ch2 = binner.channels[self.xcs1-1]
        
        b1 = ch1.counts.get()
        if len(b1) == 0:
            return self.is_running()

        b2 = ch2.counts.get()
        if len(b2) == 0:
            return self.is_running()
        
        print "shapes:",b1.shape,b2.shape
        b1 = b1[max(0,b1.shape[0]-10000):b1.shape[0]]
        b2 = b2[max(0,b2.shape[0]-10000):b2.shape[0]]
        print "tmin -- b1:{0:.4g},b2:{1:.4g}".format(np.min(b1["time"]),np.min(b2["time"]))
        print "tmax -- b1:{0:.4g},b2:{1:.4g}".format(np.max(b1["time"]),np.max(b2["time"]))
        
        xcorr = xcs(b1["counts"],b2["counts"])
        
        if len(xcorr) > 1:
            self.axes.cla()
        self.axes.plot(range(len(xcorr)),xcorr,color="blue")
        # if self.xcs0 == self.xcs1:
            # acf = acs(b1["counts"])
            # self.axes.plot(range(len(acf)),acf,color="red")
        ymax = 1.1*xcorr[1]
        print ymax
        self.axes.set_ylim(0,ymax)
        try:
            self.axes.set_xscale("log")
        except:
            pass
        self.figure.canvas.draw()
        dt = time.time() - t0
        print "execT:{1:.3g}".format(tmax,dt)
   

        return self.is_running()

    @property
    def plot_width(self):
            return self.builder.get_object('x_width').props.value

    @property
    def n_points(self):
            """ The required number of points to fill the entire
            width of the plot at the given bin_time """
            return int(self.plot_width / self.bin_time)

    @property
    def bin_time(self):
            return 1e-4# * self.builder.get_object('bin_time').props.value

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

    def rb11(self,*args):
        if args[0].get_active():
            # print "Ch11"
            self.xcs0 = 1
    def rb21(self,*args):
        if args[0].get_active():
            # print "Ch21"
            self.xcs1 = 1

    def rb12(self,*args):
        if args[0].get_active():
            # print "Ch12"
            self.xcs0 = 2

    def rb22(self,*args):
        if args[0].get_active():
            # print "Ch22"
            self.xcs1 = 2
    def rb13(self,*args):
        if args[0].get_active():
            # print "Ch13"
            self.xcs0 = 3
    def rb23(self,*args):
        if args[0].get_active():
            # print "Ch23"
            self.xcs1 = 3
    def rb14(self,*args):
        if args[0].get_active():
            # print "Ch14"
            self.xcs0 = 4
    def rb24(self,*args):
        if args[0].get_active():
            # print "Ch24"
            self.xcs1 = 4
    
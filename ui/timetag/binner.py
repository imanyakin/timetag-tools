import logging
import threading
import struct
import numpy as np
from time import time
import os
import subprocess
from collections import defaultdict
from ringbuffer import RingBuffer

bin_dtype = np.dtype([('time', 'f'), ('counts', 'u4')])

class Binner(object):
    def __init__(self, bin_time, clockrate):
        self._bin_time = bin_time
        self.clockrate = clockrate
        self.last_bin_walltime = time()
        self.latest_timestamp = 0
        self.loss_count = 0
        
        bin_length = int(bin_time * self.clockrate)
        cmd = [os.path.join('timetag_bin'), str(bin_length)]
        self._binner = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        logging.info("Started process %s" % cmd)

        self.listener = threading.Thread(name='Data Listener', target=self._listen)
        self.listener.daemon = True
        self.listener.start()

    def get_data_fd(self):
        return self._binner.stdin

    def stop(self):
        self._binner.terminate()
        self._binner = None
        self.listener.join()

    def _listen(self):
        bin_fmt = 'iQII'
        bin_sz = struct.calcsize(bin_fmt)
        while True:
            if not self._binner: break
            data = self._binner.stdout.read(bin_sz)
            if len(data) != bin_sz: break
            self.last_bin_walltime = time()

            chan, start_time, count, lost = struct.unpack(bin_fmt, data)
            self.loss_count += lost #FIXME: overcounting
            self.latest_timestamp = start_time
            self.handle_bin(chan, start_time, count, lost)

    def handle_bin(self, channel, start_time, count, lost):
        pass

class HistBinner(Binner):
    def __init__(self, bin_time, clockrate, hist_width=10):
        Binner.__init__(self, bin_time, clockrate)
        self.hist_width = hist_width

    @property
    def hist_width(self):
        return self._hist_width

    @hist_width.setter
    def hist_width(self, width):
        self._hist_width = width
        # Reset histogram
        self.channels = [defaultdict(lambda: 0) for c in range(4)]

    def handle_bin(self, channel, start_time, count, lost):
        c = self.channels[channel]
        c[int(count / self.hist_width) * self.hist_width] += 1

class FretHistBinner(Binner):
    def __init__(self, bin_time, clockrate, hist_width=10,
                 acceptor_channel=1, donor_channel=0):
        Binner.__init__(self, bin_time, clockrate)
        self.hist_width = hist_width
        self.acceptor_channel = acceptor_channel
        self.donor_channel = donor_channel
        self.threshold = 3
        self._last_donor_bin = None
        self._last_acceptor_bin = None

    @property
    def hist_width(self):
        return self._hist_width

    @hist_width.setter
    def hist_width(self, width):
        self._hist_width = width
        self.reset_hist()

    def reset_hist(self):
        self.hist = defaultdict(lambda: 0)

    def handle_bin(self, channel, start_time, count, lost):
        if channel == self.donor_channel:
            self._last_donor_bin = (start_time, count)
        elif channel == self.acceptor_channel:
            self._last_acceptor_bin = (start_time, count)
        else:
            return

        if self._last_donor_bin is None or self._last_acceptor_bin is None: return
        a_time, a_count = self._last_acceptor_bin
        d_time, d_count = self._last_donor_bin

        if d_time != a_time: return
        if a_count + d_count < self.threshold: return
        
        fret_eff = 1. * a_count / (a_count + d_count)
        self.hist[int(fret_eff / self.hist_width) * self.hist_width] += 1

class BufferBinner(Binner):
    class Channel(object):
            def __init__(self, npts):
                    self._buffer_lock = threading.Lock()
                    self.photon_count = 0
                    self.latest_timestamp = 0
                    self.resize(npts)

            def resize(self, npts):
                    self.counts = RingBuffer(npts, dtype=bin_dtype)
            
    def __init__(self, bin_time, clockrate):
        Binner.__init__(self, bin_time, clockrate)
        self.channels = [ BufferBinner.Channel(1000) for i in range(4) ]
        self.loss_count = 0

    def resize_buffer(self, npts):
        """ Creates a new bin ringbuffer. """
        logging.debug("Resizing capture ring-buffer to %d points" % npts)
        for c in self.channels:
            c.resize(npts)

    def handle_bin(self, channel, start_time, count, lost):
        c = self.channels[channel]
        start_time = 1.0*start_time / self.clockrate

        with c._buffer_lock:
            c.counts.append((start_time, count))
            c.photon_count += count
            c.latest_timestamp = start_time

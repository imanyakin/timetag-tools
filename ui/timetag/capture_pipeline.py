# vim: set fileencoding=utf-8 et :

# timetag-tools - Tools for UMass FPGA timetagger
# 
# Copyright © 2010 Ben Gamari
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see http://www.gnu.org/licenses/ .
# 
# Author: Ben Gamari <bgamari@physics.umass.edu>
# 

import socket
import subprocess
import logging
import sys
import struct
import random
from time import sleep, time
import os
import threading
from collections import defaultdict
from ringbuffer import RingBuffer

logging.basicConfig(level=logging.DEBUG)
control_sock = '/tmp/timetag.sock'

class CapturePipeline(object):
        class Channel(object):
                def __init__(self, npts):
                        self.buffer_lock = threading.Lock()
                        self.times = RingBuffer(npts)
                        self.counts = RingBuffer(npts)
                        self.loss_count = 0
                        self.photon_count = 0
                        self.latest_timestamp = 0
                        self.hist = defaultdict(lambda: 0)

        def __init__(self, bin_time, npts, output_file=None):
                """ Create a capture pipeline. The bin_time is given in
                    seconds. """
                self.resize_buffer(npts)
                self._bin_time = bin_time
                self.output_file = output_file
                self.last_bin_walltime = time()
                self.latest_timestamp = 0
                self._hist_width = 10

        @property
        def bin_time(self):
                return self._bin_time

        @property
        def hist_width(self):
                return self._hist_width

        @hist_width.setter
        def hist_width(self, width):
                self._hist_width = width
                # Reset histogram
                for c in self.channels:
                        self.hist = defaultdict(lambda: 0)

        def bins(self):
                for n, chan in self.channels.items():
                        with chan.buffer_lock:
                                yield n, chan.times.get(), chan.counts.get()

        def stats(self):
                for n, chan in self.channels.items():
                        yield n, chan.photon_count, chan.loss_count, chan.latest_timestamp

        def resize_buffer(self, npts):
                """ Creates a new bin ringbuffer. """
                logging.debug("Resizing capture ring-buffer to %d points" % npts)
                self.channels = defaultdict(lambda: CapturePipeline.Channel(npts))

        def start(self):
                PIPE=subprocess.PIPE
                cmd = ['timetag_acquire', control_sock]
                self.source = subprocess.Popen(cmd, stdout=PIPE)
                logging.info("Started process %s" % cmd)
                self.control_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM, 0)
                for i in range(10):
                        sleep(0.05)
                        try: self.control_sock.connect(control_sock)
                        except: pass
                        else: break

                sleep(0.5)
                self.control = self.control_sock.makefile('rw', 0)
                l = self.control.readline() # Read "ready"
                if l.strip() != "ready":
                        raise RuntimeError('Invalid status message: %s' % l)

                self.clockrate = int(self._tagger_cmd('clockrate\n'))
                logging.info('Tagger clockrate: %f MHz' % (self.clockrate / 1e6))
                self.hw_version = self._tagger_cmd('version\n')
                logging.info('Tagger HW version: %s' % self.hw_version)

                self.stop_capture()
                self.reset_counter()

                src = self.source.stdout
                if self.output_file:
                        self.tee = subprocess.Popen(['tee', self.output_file], stdin=src, stdout=PIPE)
                        src = self.tee.stdout
                else:
                        self.tee = None

                self.bin_length = int(self.bin_time * self.clockrate)
                cmd = [os.path.join('timetag_bin'), str(self.bin_length)]
                self.binner = subprocess.Popen(cmd, stdin=src, stdout=PIPE)
                logging.info("Started process %s" % cmd)

                self.listener = threading.Thread(name='Data Listener', target=self._listen)
                self.listener.daemon = True
                self.listener.start()

        def _tagger_cmd(self, cmd):
                if self.source.poll() is not None:
                        logging.error('Tagger subprocess died (exit code %d)' %
                                        self.source.returncode)

                logging.debug("Tagger command: %s" % cmd.strip())
                self.control.write(cmd)
                result = None
                while True:
                        l = self.control.readline().strip()
                        if l.startswith('= '):
                                result = l[2:]
                                l = self.control.readline().strip()
                        if l == 'ready':
                                break
                        else:
                                raise RuntimeError('Invalid status message: %s' % l)
                return result

        def _listen(self):
                bin_fmt = 'iQII'
                bin_sz = struct.calcsize(bin_fmt)
                while True:
                        data = self.binner.stdout.read(bin_sz)
                        if len(data) != bin_sz: break
                        self.last_bin_walltime = time()
                        chan, start_time, count, lost = struct.unpack(bin_fmt, data)
                        c = self.channels[chan]
                        start_time = 1.0*start_time / self.clockrate

                        with c.buffer_lock:
                                c.times.append(start_time)
                                c.counts.append(count)
                                c.hist[int(count / self.hist_width) * self.hist_width] += 1

                        c.photon_count += count
                        c.loss_count += lost
                        self.latest_timestamp = c.latest_timestamp = start_time

        def stop(self):
                if self.source.poll() is not None: return
                logging.info("Capture pipeline shutdown")
                self.control.write('quit\n')
                self.control.close()
                for i in range(40):
                        if self.source.poll() is not None: return
                        sleep(0.05)

                logging.error('Failed to shutdown timetag_acquire')
                self.source.terminate()

        def reset_counter(self):
                self._tagger_cmd('reset_counter\n')

        def stop_capture(self):
                self._tagger_cmd('stop_capture\n')

        def start_capture(self):
                self._tagger_cmd('start_capture\n')

        def set_send_window(self, window):
                self._tagger_cmd('set_send_window %d\n' % window)

class TestPipeline(object):
        def __init__(self, npts=10):
                self.resize_buffer(npts)
                self.count_totals = [0,0]
                self.latest_timestamp = self.t = 0
                self.set_hist_width(10)
                self.clockrate = 30e6

        def set_hist_width(self, width):
                self.hist_width = width
                class Channel(object):
                        def __init__(self):
                                self.hist = defaultdict(lambda: 0)
                self.channels = [Channel(), Channel()]

        def bins(self):
                for i,c in enumerate(self.counts):
                        yield 0, self.times.get(), c.get()

        def resize_buffer(self, npts):
                """ Creates a new bin ringbuffer. """
                logging.debug("Resizing capture ring-buffer to %d points" % npts)
                self.times = RingBuffer(npts)
                self.counts = []
                self.counts.append(RingBuffer(npts))
                self.counts.append(RingBuffer(npts))

        def stats(self):
                for i,c in enumerate(self.counts):
                        yield 0, self.count_totals[i], 0, self.t

        def start(self):
                pass
        
        def worker(self):
                while self._running:
                        self.times.append(self.t)
                        for i,c in enumerate(self.counts):
                                count = int(random.gauss(50, 10))
                                self.counts[i].append(count)
                                self.count_totals[i] += count
                                self.channels[i].hist[int(count / self.hist_width) * self.hist_width] += 1

                        self.latest_timestamp = self.t
                        self.t += 1.0/40
                        sleep(1.0/40)

        def stop(self):
                self._running = False

        def start_capture(self):
                self._running = True
                self.listener = threading.Thread(name='Test Data Generator', target=self.worker)
                self.listener.daemon = True
                self.listener.start()

        def stop_capture(self):
                logging.info('stop_capture')
                self._running = False

        def set_send_window(self, window):
                logging.info('set_send_widow %d' % window)

        def reset_counter(self):
                self.t = 0
                logging.info('reset_counter')

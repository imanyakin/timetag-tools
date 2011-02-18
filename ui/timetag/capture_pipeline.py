# vim: set fileencoding=utf-8

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

import timetag_interface

logging.basicConfig(level=logging.DEBUG)

class CapturePipeline(object):
        class Channel(object):
                def __init__(self, npts):
			self.buffer_lock = threading.Lock()
                        self.times = RingBuffer(npts)
                        self.counts = RingBuffer(npts)
                        self.loss_count = 0
                        self.photon_count = 0
                        self.latest_timestamp = 0

        def bins(self):
                for n, chan in self.channels.items():
			chan.buffer_lock.acquire()
                        yield n, chan.times.get(), chan.counts.get()
			chan.buffer_lock.release()

        def stats(self):
                for n, chan in self.channels.items():
                        yield n, chan.photon_count, chan.loss_count, chan.latest_timestamp

        def resize_buffer(self, npts):
                """ Creates a new bin ringbuffer. """
                logging.debug("Resizing capture ring-buffer to %d points" % npts)
                self.channels = defaultdict(lambda: CapturePipeline.Channel(npts))
		
        def __init__(self, bin_time, npts, capture_clock, output_file=None):
                """ Create a capture pipeline. The bin_time is given in
                    seconds. capture_clock given in Hz. """
                self.capture_clock = capture_clock
                self.resize_buffer(npts)
                self.bin_length = int(bin_time * self.capture_clock)
                self.output_file = output_file
                self.last_bin_walltime = time()
                self.latest_timestamp = 0

        def start(self):
                #cmd = ['photon_generator', str(1000)]
                cmd = ['timetag_acquire']

                PIPE=subprocess.PIPE
                self.source = subprocess.Popen(cmd, stdin=PIPE, stdout=PIPE)
                logging.info("Started process %s" % cmd)
                src = self.source.stdout
                
                if self.output_file:
                        self.tee = subprocess.Popen(['tee', self.output_file], stdin=src, stdout=PIPE)
                        src = self.tee.stdout
                else:
                        self.tee = None

                cmd = [os.path.join('timetag_bin'), str(self.bin_length)]
                self.binner = subprocess.Popen(cmd, stdin=src, stdout=PIPE)
                logging.info("Started process %s" % cmd)

                self.listener = threading.Thread(name='Data Listener', target=self._listen)
                self.listener.daemon = True
                self.listener.start()

		self.tagger_cmd('reset_counter')

	def _tagger_cmd(self, cmd):
		logging.debug("Tagger command: %s" % cmd)
		self.source.stdin.write(cmd)

        def _listen(self):
                bin_fmt = 'iQII'
                bin_sz = struct.calcsize(bin_fmt)
                while True:
                        data = self.binner.stdout.read(bin_sz)
                        if len(data) != bin_sz: break
                        self.last_bin_walltime = time()
                        chan, start_time, count, lost = struct.unpack(bin_fmt, data)
                        c = self.channels[chan]
                        start_time = 1.0*start_time / self.capture_clock
			c.buffer_lock.acquire()
                        c.times.append(start_time)
                        c.counts.append(count)
			c.buffer_lock.release()
                        c.photon_count += count
                        c.loss_count += lost
                        self.latest_timestamp = c.latest_timestamp = start_time

        def stop(self):
                logging.info("Capture pipeline shutdown")
                self.source.terminate()

        def __del__(self):
                self.stop()

	def stop_capture(self):
		self.tagger_cmd('stop_capture\n')

	def start_capture(self):
		self.tagger.cmd('start_capture\n')

	def set_send_window(self, window):
		self.tagger.cmd('set_send_window %d\n' % window)

class TestPipeline(object):
        def __init__(self, npts=10):
                self.times = RingBuffer(npts)
                self.counts = []
                self.counts.append(RingBuffer(npts))
                self.counts.append(RingBuffer(npts))

                self.count_totals = [0,0]
                self.t = 0

        def bins(self):
                for i,c in enumerate(self.counts):
                        yield 0, self.times.get(), c.get()

        def stats(self):
                for i,c in enumerate(self.counts):
                        yield 0, self.count_totals[c], 0, self.t

        def start(self):
                self._running = True
                self.listener = threading.Thread(name='Test Data Listener', target=self.worker)
                self.listener.daemon = True
                self.listener.start()
        
        def worker(self):
                while self._running:
                        self.times.append(self.t)
                        for i,c in enumerate(self.counts):
                                count = random.randint(0, 200)
                                self.counts[i].append(count)
                                self.count_totals[i] += count

                        self.t += 1.0/40
                        sleep(1.0/40)

        def stop(self):
                self._running = False


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
import passfd
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

        def __init__(self, control_sock='/tmp/timetag.sock'):
                """ Create a capture pipeline. The bin_time is given in
                    seconds. """
                self.resize_buffer(10)
                self.last_bin_walltime = time()
                self.latest_timestamp = 0
                self.loss_count = 0
                self._hist_width = 10
                self._binner = None
                self._out_file = None

                self._control_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM, 0)
                for i in range(10):
                        sleep(0.05)
                        try: self._control_sock.connect(control_sock)
                        except: pass
                        else: break

                sleep(0.5)
                self._control = self._control_sock.makefile('rw', 0)
                l = self._control.readline() # Read "ready"
                if l.strip() != "ready":
                        raise RuntimeError('Invalid status message: %s' % l)

                self.clockrate = int(self._tagger_cmd('clockrate?\n'))
                logging.info('Tagger clockrate: %f MHz' % (self.clockrate / 1e6))
                self.hw_version = self._tagger_cmd('version?\n')
                logging.info('Tagger HW version: %s' % self.hw_version)

                self.stop_capture()
                self.reset_counter()

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
                        yield n, chan.photon_count, chan.latest_timestamp

        def resize_buffer(self, npts):
                """ Creates a new bin ringbuffer. """
                logging.debug("Resizing capture ring-buffer to %d points" % npts)
                self.channels = defaultdict(lambda: CapturePipeline.Channel(npts))

        def start_output_file(self, filename):
                assert not self._out_file
                self._out_file = open(filename, 'w')
                self._control.write('add_output file\n')
                sleep(0.01) # HACK: Otherwise the packet gets lost
                passfd.sendfd(self._control_sock, self._out_file)
                self._read_reply()

        def stop_output_file(self):
                if not self._out_file:
                        logging.warning('Tried to stop output file when one has not been started')
                        return
                self._tagger_cmd('remove_output file\n')
                self._out_file = None

        def start_binner(self, bin_time):
                if self._binner:
                        logging.warning('Tried to start binner when one is already running')
                        return
                bin_length = int(bin_time * 1e-3 * self.clockrate)
                cmd = [os.path.join('timetag_bin'), str(bin_length)]
                self._binner = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
                self._control.write('add_output binner\n')
                self._control.flush()
                sleep(0.01) # HACK: Otherwise the packet gets lost
                passfd.sendfd(self._control_sock, self._binner.stdin)
                self._read_reply()
                logging.info("Started process %s" % cmd)
                self.resize_buffer(1000) # FIXME

                self.listener = threading.Thread(name='Data Listener', target=self._listen)
                self.listener.daemon = True
                self.listener.start()

        def stop_binner(self):
                if not self._binner:
                        logging.warning('Tried to stop binner when one is not running')
                        return
                self._tagger_cmd('remove_output binner\n')
                self._binner.terminate()
                self._binner = None
                self.listener.join()

        def _tagger_cmd(self, cmd):
                logging.debug("Tagger command: %s" % cmd.strip())
                self._control.write(cmd)
                return self._read_reply(cmd)

        def _read_reply(self, cmd=''):
                result = None
                while True:
                        l = self._control.readline().strip()
                        if l.startswith('= '):
                                result = l[2:]
                                l = self._control.readline().strip()
                        if l == 'ready':
                                break
                        if l.startswith('error'):
                                logging.error('Timetagger error while handling command: %s' % cmd)
                                logging.error('Error: %s' % l)
                                raise RuntimeError
                        else:
                                logging.error('Invalid status message: %s' % l)
                                raise RuntimeError
                return result

        def _listen(self):
                bin_fmt = 'iQII'
                bin_sz = struct.calcsize(bin_fmt)
                while True:
                        if not self._binner: break
                        data = self._binner.stdout.read(bin_sz)
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
                        self.loss_count += lost #FIXME: overcounting
                        self.latest_timestamp = c.latest_timestamp = start_time

        def stop(self):
                logging.info("Capture pipeline shutdown")
                self._control.write('quit\n')
                self._control.close()

        def reset_counter(self):
                self._tagger_cmd('reset_counter\n')

        def stop_capture(self):
                self._tagger_cmd('stop_capture\n')

        def start_capture(self):
                self._tagger_cmd('flush_fifo\n')
                self._tagger_cmd('start_capture\n')

        def set_send_window(self, window):
                self._tagger_cmd('set_send_window %d\n' % window)

class TestPipeline(object):
        def __init__(self, npts=10):
                self.resize_buffer(npts)
                self.count_totals = [0,0]
                self.loss_count = 0
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
                        yield 0, self.count_totals[i], self.t

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

        def start_binner(self, bin_time):
                pass
        
        def stop_binner(self):
                pass

        def set_send_window(self, window):
                logging.info('set_send_widow %d' % window)

        def reset_counter(self):
                self.t = 0
                logging.info('reset_counter')

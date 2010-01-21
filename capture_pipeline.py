import subprocess
import logging
import sys
import random
from time import sleep, time
import os
import threading
from collections import defaultdict

import timetag_interface

logging.basicConfig(level=logging.DEBUG)

CAPTURE_CLOCK=30e6 # Hz
bin_root = "/usr/bin"

class RingBuffer:
	def __init__(self, size_max):
		self._cur = 0
		self._size = size_max
		self._data = []

	def append(self, x):
		"""append an element at the end of the buffer"""
		self._data.append(x)
		if len(self._data) == self._size:
			self._cur = 0
			self.__class__ = RingBuffer.RingBufferFull
	def get(self):
  		""" return a list of elements from the oldest to the newest"""
		return self._data


        class RingBufferFull:
                def append(self,x):		
                        self._data[self._cur] = x
                        self._cur = int((self._cur+1) % self._size)

                def get(self):
                        return self.data[self._cur:] + self.data[:self._cur]
		

class CapturePipeline(object):
        class Channel(object):
                def __init__(self, npts):
                        self.times = RingBuffer(npts)
                        self.counts = RingBuffer(npts)
                        self.loss_count = 0
                        self.photon_count = 0
                        self.latest_timestamp = 0

        def bins(self):
                for n, chan in self.channels.items():
                        yield n, chan.times.get(), chan.counts.get()

        def stats(self):
                for n, chan in self.channels.items():
                        yield n, chan.photon_count, chan.loss_count, chan.latest_timestamp

        def __init__(self, bin_time, npts, output_file=None):
                """ Create a capture pipeline. The bin_time is given in seconds """
                self.channels = defaultdict(lambda: CapturePipeline.Channel(npts))

                self.bin_length = int(bin_time * CAPTURE_CLOCK)
                self.output_file = output_file
                self.update_cb = None
                self.last_bin_walltime = time()

        def start(self):
                #cmd = [os.path.join(bin_root, 'photon_generator'), str(1000)]
                cmd = [os.path.join(bin_root, 'timetag_acquire')]

                PIPE=subprocess.PIPE
                self.source = subprocess.Popen(cmd, stdin=PIPE, stdout=PIPE)
                logging.info("Started process %s" % cmd)
                src = self.source.stdout
                
                if self.output_file:
                        self.tee = subprocess.Popen(['tee', self.output_file], stdin=src, stdout=PIPE)
                        src = self.tee.stdout
                else:
                        self.tee = None

                cmd = [os.path.join(bin_root, 'bin_photons'), str(self.bin_length)]
                self.binner = subprocess.Popen(cmd, stdin=src, stdout=PIPE)
                logging.info("Started process %s" % cmd)

                self.listener = threading.Thread(name='Data Listener', target=self._listen)
                self.listener.daemon = True
                self.listener.start()

                self.tagger = timetag_interface.Timetag(self.source.stdin)

        def _listen(self):
                while True:
                        l = self.binner.stdout.readline()
                        #print l
                        self.last_bin_walltime = time()
                        if len(l) == 0: 
				logging.debug("Listener reached end of stream. Stopping...")
				return
                        chan, start_time, count, lost = map(int, l.split())
                        c = self.channels[chan]
                        start_time = 1.0*start_time / CAPTURE_CLOCK
                        c.times.append(start_time)
                        c.counts.append(count)
                        c.photon_count += count
                        c.loss_count += lost
                        c.latest_timestamp = start_time

        def stop(self):
                logging.info("Capture pipeline shutdown")
                self.tagger = None
                self.source.terminate()

        def __del__(self):
                self.stop()


class TestPipeline(object):
        def __init__(self, npts=10):
                self.times = RingBuffer(npts)
                self.counts = []
                self.counts.append(RingBuffer(npts))
                self.counts.append(RingBuffer(npts))

                self.count_totals = [0,0]
                self.t = 0
                self.tagger = timetag_interface.Timetag(sys.stderr)

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


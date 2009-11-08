import subprocess
import logging
import random
import time
import threading
from collections import defaultdict

class RingBuffer:
	def __init__(self,size_max):
		self.max = size_max
		self.data = []

	def append(self,x):
		"""append an element at the end of the buffer"""
		self.data.append(x)
		if len(self.data) == self.max:
			self.cur = 0
			self.__class__ = RingBuffer.RingBufferFull
	def get(self):
  		""" return a list of elements from the oldest to the newest"""
		return self.data


        class RingBufferFull:
                def append(self,x):		
                        self.data[self.cur] = x
                        self.cur = (self.cur+1) % self.max

                def get(self):
                        return self.data[self.cur:] + self.data[:self.cur]
		

class CapturePipeline(object):
        CAPTURE_CLOCK=150e6 # Hz
        class Channel(object):
                def __init__(self, npts):
                        self.times = RingBuffer(npts)
                        self.counts = RingBuffer(npts)

        def __iter__(self):
                for n, chan in self.channels.items():
                        yield n, chan.times.get(), chan.counts.get()
                        return 

        def __init__(self, bin_length=100e6, output_file=None, points=100):
                """ Create a capture pipeline. The bin length is given in timer units. """
                self.channels = defaultdict(lambda: CapturePipeline.Channel(points))

                self.bin_length = int(bin_length)
                self.output_file = output_file
                self.update_cb = None

        def start(self):
                PIPE=subprocess.PIPE
                cmd = ['./photon_generator', str(100)]
                self.source = subprocess.Popen(cmd, stdout=PIPE)
                logging.info("Started process %s" % cmd)
                src = self.source.stdout
                
                if self.output_file:
                        self.tee = subprocess.Popen(['tee', self.output_file], stdin=src, stdout=PIPE)
                        src = self.tee.stdout
                else:
                        self.tee = None

                cmd = ['./bin_photons', str(self.bin_length)]
                self.binner = subprocess.Popen(cmd, stdin=src, stdout=PIPE)
                logging.info("Started process %s" % cmd)

                self.listener = threading.Thread(name='Data Listener', target=self.listen)
                self.listener.daemon = True
                self.listener.start()

        def listen(self):
                while True:
                        l = self.binner.stdout.readline()
                        if len(l) == 0: return
                        chan, start_time, count, = map(int, l.split())
                        c = self.channels[chan]
                        c.times.append(1.0*start_time)# / CapturePipeline.CAPTURE_CLOCK)
                        c.counts.append(count)
                        if self.update_cb: self.update_cb()

                        #if chan==1 and c.idx % 20 == 0: print c.idx, c.times, c.counts

        def stop(self):
                self.source.terminate()
                logging.info("Capture pipeline shutdown")

        def __del__(self):
                self.stop()


class TestPipeline(object):
        def __init__(self, npts=10):
                self.times = RingBuffer(npts)
                self.counts = RingBuffer(npts)
                self.t = 0
                self.update_cb = None

        def __iter__(self):
                yield 0, self.times.get(), self.counts.get()

        def start(self):
                self._running = True
                self.listener = threading.Thread(name='Test Data Listener', target=self.worker)
                self.listener.daemon = True
                self.listener.start()
        
        def worker(self):
                while self._running:
                        self.times.append(self.t)
                        self.counts.append(random.randint(0, 200))
                        self.t += 10
                        
                        if self.update_cb: self.update_cb()
                        time.sleep(1.0/30)

        def stop(self):
                self._running = False

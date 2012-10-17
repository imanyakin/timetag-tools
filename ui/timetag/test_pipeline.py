import logging
import threading
from time import sleep
import struct
import numpy.random

class TestPipeline(object):
        def __init__(self, clockrate=32e6):
                self.count_rates = [ 100, 80, 0, 0 ]
                self.clockrate = clockrate
                self.hw_version = '1'
                self._outputs = {}

        def _emit_record(self, channel, time):
                a = bytearray([0,
                               (time >> 32) & 0x0f | (channel & 0xf) << 4,
                               (time >> 24) & 0xff,
                               (time >> 16) & 0xff,
                               (time >>  8) & 0xff,
                               (time >>  0) & 0xff
                               ])
                for id,file in self._outputs.items():
                    file.write(a)
                    
        def _worker(self, channel, rate):
                t = 0
                while self._running:
                    accum = 0
                    while accum < 1e-1:
                        dt = numpy.random.exponential(1./rate)
                        accum += dt
                        t += int(self.clockrate*dt)
                        self._emit_record(channel, t)
                    sleep(accum)

        def stop(self):
                self._running = False

        def start_capture(self):
                self._running = True
                self.listener = threading.Thread(name='Test Data Generator',
                                                 target=self._worker,
                                                 args=(1, self.count_rates[0]))
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

        def add_output(self, id, file):
                self._outputs[id] = file

        def remove_output(self, id):
                self._outputs.pop(id)

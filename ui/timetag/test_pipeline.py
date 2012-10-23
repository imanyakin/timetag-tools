import logging
import threading
from time import sleep
import struct
import numpy.random

def categorical(xs):
        a = numpy.random.rand()
        for (p,x) in xs:
                if p > a: return x
                a -= p
        return x

class TestPipeline(object):
        def __init__(self, clockrate=32e6):
                self.count_rates = [ 2000, 2000, 0, 0 ]
                self.clockrate = clockrate
                self.hw_version = '1'
                self._outputs = {}
                self._running = False

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
                    
        def _worker(self, count_rates):
                t = 0
                sum_rate = sum(count_rates)
                while self._running:
                    accum = 0
                    while accum < 1e-1:
                        dt = numpy.random.exponential(1./sum_rate)
                        channel = categorical((1. * rate / sum_rate, ch+1)
                                              for (ch,rate) in enumerate(count_rates))
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
                                                 args=(self.count_rates,))
                self.listener.daemon = True
                self.listener.start()

        def stop_capture(self):
                logging.info('stop_capture')
                self._running = False

        def is_capture_running(self):
                return self._running

        def set_send_window(self, window):
                logging.info('set_send_widow %d' % window)

        def reset_counter(self):
                self.t = 0
                logging.info('reset_counter')

        def add_output(self, id, file):
                self._outputs[id] = file

        def remove_output(self, id):
                self._outputs.pop(id)

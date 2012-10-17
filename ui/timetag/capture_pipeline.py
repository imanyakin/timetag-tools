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
import logging
import sys
from time import sleep

logging.basicConfig(level=logging.DEBUG)

class CapturePipeline(object):
        def __init__(self, control_sock='/tmp/timetag.sock'):
                """ Create a capture pipeline. The bin_time is given in
                    seconds. """
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

        def add_output(self, id, file):
                self._control.write('add_output %s\n' % id)
                sleep(0.01) # HACK: Otherwise the packet gets lost
                passfd.sendfd(self._control_sock, file)
                self._read_reply()

        def remove_output(self, id):
                self._tagger_cmd('remove_output %s\n' % id)


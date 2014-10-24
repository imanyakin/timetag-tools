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
import zmq
import logging
import sys
from time import sleep

logging.basicConfig(level=logging.DEBUG)

class CapturePipeline(object):
        def __init__(self):
                """ Create a capture pipeline. The bin_time is given in
                    seconds. """
                self._zmq = zmq.Context().instance()
                self._ctrl_sock = self._zmq.socket(zmq.REQ)
                self._ctrl_sock.connect('ipc:///tmp/timetag-ctrl')

                self.clockrate = int(self._tagger_cmd('clockrate?'))
                logging.info('Tagger clockrate: %f MHz' % (self.clockrate / 1e6))
                self.hw_version = self._tagger_cmd('version?')
                logging.info('Tagger HW version: %s' % self.hw_version)

        def _tagger_cmd(self, cmd):
                logging.debug("Tagger command: %s" % cmd.strip())
                self._ctrl_sock.send_string(cmd)
                return self._ctrl_sock.recv_string()

        def stop_capture(self):
                self._tagger_cmd('stop_capture')

        def start_capture(self):
                self._tagger_cmd('reset_counter')
                self._tagger_cmd('start_capture')

        def is_capture_running(self):
                return bool(int(self._tagger_cmd('capture?')))

        def set_send_window(self, window):
                self._tagger_cmd('set_send_window %d' % window)

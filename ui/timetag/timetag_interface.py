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


import logging

class Timetag(object):
        def __init__(self, file):
                self.fd = file

	def _submit_cmd(self, cmd):
		logging.debug(cmd[:-1])
		self.fd.write(cmd)

        def start_timer(self):
                self._submit_cmd("start_timer\n")

        def stop_timer(self):
                self._submit_cmd("stop_timer\n")

        def start_capture(self):
                self._submit_cmd("start_capture\n")

        def stop_capture(self):
                self._submit_cmd("stop_capture\n")

	def reset(self):
		self._submit_cmd("reset\n")

	def reset_counter(self):
		self._submit_cmd("reset_counter\n")

	def set_send_window(self, window):
		self._submit_cmd("set_send_window %d\n" % window)

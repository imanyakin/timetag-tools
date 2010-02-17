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


from array import array

class RingBuffer:
	def __init__(self, length, fmt='f'):
		self._cur = 0
		self._size = int(length)
		self._data = array(fmt, [0] * int(length))

	def append(self, x):
		"""append an element at the end of the buffer"""
		self._data[self._cur] = x
                self._cur += 1
		if self._cur == self._size:
			self._cur = 0
			self.__class__ = RingBuffer.RingBufferFull

	def get(self):
  		""" return a list of elements from the oldest to the newest"""
		return self._data[:self._cur]

        def get_unordered(self):
                return self._data[:self._cur]


        class RingBufferFull:
                def append(self, x):
                        self._data[self._cur] = x
                        self._cur = int((self._cur+1) % self._size)

                def get(self):
                        return self._data[self._cur:] + self._data[:self._cur]
                
                def get_unordered(self):
                        return self.data

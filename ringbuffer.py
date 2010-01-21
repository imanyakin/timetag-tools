from array import array

class RingBuffer:
	def __init__(self, length, fmt='f'):
		self._cur = 0
		self._size = length
		self._data = array.array(fmt, [0] * length)

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
                        return self.data[self._cur:] + self.data[:self._cur]
                
                def get_unordered(self):
                        return self.data

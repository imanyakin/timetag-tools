#!/usr/bin/python

from distutils.core import setup

setup(name = 'timetag_ui',
      author = 'Ben Gamari',
      author_email = 'bgamari@physics.umass.edu',
      url = 'http://goldnerlab.physics.umass.edu/',
      description = 'User interface for acquiring data with FPGA timetagger',
      version = '1.0',
      packages = ['timetag'],
      scripts = ['timetag_ui', 'timetag_seq_ui'],
      package_data = {'timetag': ['main.glade', 'bin_series.glade', 'hist.glade', 'default.cfg']},
      license = 'GPLv3',
)

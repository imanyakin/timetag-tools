from distutils.core import setup

setup(name = 'timetag_ui',
      author = 'Ben Gamari',
      author_email = 'bgamari@physics.umass.edu',
      url = 'http://goldnerlab.physics.umass.edu/',
      description = 'User interface for acquiring data with FPGA timetagger',
      version = '1.0',
      packages = ['timetag'],
      scripts = ['timetag_ui'],
      data_files = [('share/timetag', ['timetag_ui.glade', 'default.cfg'])],
      license = 'GPLv3',
)

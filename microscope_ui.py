#!/usr/bin/python

import gtk
import matplotlib.pyplot as plt
import matplotlib.backends.gtk as plot_backend


class MainWindow(object):
        def __init__(self):
                self.win = gtk.Builder('microscope_ui.glade')
                self.show_all()

if __name__ == '__main__':
        win = MainWindow()
        gtk.main_loop()

import zmq
import subprocess
import threading
import logging

class ManagedBinner(object):
    POLL_PERIOD = 2
    def __init__(self, pipeline, name='managed_binner'):
        self._cat = None
        self._binner = None

        self._zmq = zmq.Context.instance()

        # Connect to control socket
        self._ctrl_sock = self._zmq.socket(zmq.REQ)
        self._ctrl_sock.connect('ipc:///tmp/timetag-ctrl')

        # Start watching for changes
        self._event_sock = self._zmq.socket(zmq.SUB)
        self._event_sock.connect('ipc:///tmp/timetag-event')
        self._event_sock.setsockopt(zmq.SUBSCRIBE, '')
        self._watch_thread = threading.Thread(target=self._watch)
        self._watch_thread.daemon = True
        self._watch_thread.start()

        self.restart_binner()
    
    def _watch(self):
        while True:
            s = self._event_sock.recv_string()
            if s.startswith('capture start'):
                self._start_binner()
            elif s.startswith('capture stop'):
                self._stop_binner()

    def _start_binner(self):
        if self._binner is not None:
            logging.warn("Binner already started")
            return
        self._binner = self.create_binner()
        self._cat = subprocess.Popen(['timetag-cat'], stdout=self._binner.get_data_fd())
        self.on_started()

    def _stop_binner(self):
        # Stop binner
        if self._binner is None:
            logging.warn("Binner already stopped")
        else:
            self._binner.stop()
            self._binner = None

        # Remove output
        if self._cat is None:
            logging.warn("No associated output")
        else:
            self._cat.terminate()
            self._cat = None

        self.on_stopped()

    def stop_binner(self):
        if self._binner is not None:
            self._stop_binner()

    def restart_binner(self):
        self.stop_binner()

        # See if things are already running
        self._ctrl_sock.send_string('capture?')
        reply = self._ctrl_sock.recv_string()
        if int(reply):
            self._start_binner()

    def get_binner(self):
        return self._binner

    def create_binner(self):
        raise NotImplemented("ManagedBinner is an abstract class")

    def is_running(self):
        return self._binner is not None

    def on_started(self): pass
    def on_stopped(self): pass

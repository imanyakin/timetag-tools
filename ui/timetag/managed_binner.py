import logging
import gobject

class ManagedBinner(object):
    POLL_PERIOD = 2
    def __init__(self, pipeline, name='managed_binner'):
        self._pipeline = pipeline
        self._binner = None
        self._binner_name = '%s:%x' % (name,id(self))
        self._pipeline.start_notifiers.append(self._start_binner)
        self._pipeline.stop_notifiers.append(self._stop_binner)
        self._watch()
        gobject.timeout_add_seconds(ManagedBinner.POLL_PERIOD, self._watch)
    
    def _watch(self):
        if self._pipeline.is_capture_running() and self._binner is None:
            self._start_binner()
        elif not self._pipeline.is_capture_running() and self._binner is not None:
            self._stop_binner()
        return True

    def _start_binner(self):
        if self._binner is not None:
            logging.warn("Binner already started")
            return
        self._binner = self.create_binner()
        self._pipeline.add_output(self._binner_name, self._binner.get_data_fd())
        self.on_started()

    def _stop_binner(self):
        if self._binner is None:
            logging.warn("Binner already stopped")
            return
        self._pipeline.remove_output(self._binner_name)
        self._binner.stop()
        self._binner = None
        self.on_stopped()

    def get_binner(self):
        return self._binner

    def create_binner(self):
        raise NotImplemented("ManagedBinner is an abstract class")

    def is_running(self):
        return self._binner is not None

    def on_started(self): pass
    def on_stopped(self): pass

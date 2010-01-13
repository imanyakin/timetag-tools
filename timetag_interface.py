class Timetag(object):
        def __init__(self, file):
                self.fd = file
                
        def start_outputs(self, outputs=[0,1,2,3]):
                self.fd.write("start_outputs %d %d %d %d\n" % 
                        (0 in outputs, 1 in outputs, 2 in outputs, 3 in outputs)

        def stop_outputs(self, outputs=[0,1,2,3]):
                self.fd.write("stop_outputs %d %d %d %d\n" % 
                        (0 in outputs, 1 in outputs, 2 in outputs, 3 in outputs)

        def set_initial_state(self, mask, state):
                self.fd.write("set_initial_state %d %d\n" % (mask, state))

        def set_initial_count(self, mask, count):
                self.fd.write("set_initial_count %d %d\n" % (mask, count))

        def set_high_count(self, mask, count):
                self.fd.write("set_high_count %d %d\n" % (mask, count))

        def set_low_count(self, mask, count):
                self.fd.write("set_low_count %d %d\n" % (mask, count))

        def start_capture(self):
                self.fd.write("start_capture\n")

        def stop_capture(self):
                self.fd.write("stop_capture\n")


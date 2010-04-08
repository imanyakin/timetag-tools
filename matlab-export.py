#!/usr/bin/python

import sys
import pytimetag
import scipy, scipy.io

in_file = sys.argv[1]
f = open(in_file, 'r')

stream = pytimetag.Stream(f.fileno)
l = []
while True:
        try:
                r = stream.get_record()
                a = [r.time, r.type, r.wrap_flag, r.lost_flag ]
                for i in range(4):
                        a.append(r.get_channel(i))
                l.append(a)
        except EOFError:
                break

l = scipy.array(l)

out_file = in_file.replace(".timetag", "")
out_file += ".mat"

d = { 'records': l }
scipy.io.savemat(out_file, d)

f.close()

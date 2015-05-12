#!/usr/bin/env python

import numpy as np
import os as os
import scikits.audiolab as alab

#generate test input
ref = np.linspace(-1, 1, 200)
alab.flacwrite(ref, 'test_in.flac', 48000, 'pcm24')

#create reference output
expected = ref*(10**(-1.0/20))

#run file-qdsp
os.system("../file-qdsp -n 64 -i test_in.flac -o test_out.flac -p gain,g=-1")

#read result file
res = alab.flacread("test_out.flac")[0]

#compare results
if (abs(expected - res) > 1e-6).any():
    print "fail"
else:
    print "pass"



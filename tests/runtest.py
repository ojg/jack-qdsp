#!/usr/bin/env python

from numpy import *
import os as os
import scikits.audiolab as alab

def writeaudio(data):
    alab.wavwrite(data, 'test_in.wav', 48000, 'float32')


def readaudio():
    return alab.wavread("test_out.wav")[0]


def compareaudio(data1, data2):
    if (abs(data1 - data2) > 1e-7).any():
        print "Fail"
        quit()
    else:
        print "Pass"


def test_gain():
    print "Testing dsp-gain"

    #generate test input
    ref = linspace(-1, 1, 200)
    writeaudio(ref)

    #create reference output
    expected = ref*(10**(-1.0/20))
    
    #run file-qdsp
    os.system("../file-qdsp -n 64 -i test_in.wav -o test_out.wav -p gain,g=-1")
    
    #compare results
    compareaudio(expected, readaudio())

    expected = concatenate((zeros(48), ref[0:-48]))
    os.system("../file-qdsp -n 64 -i test_in.wav -o test_out.wav -p gain,d=0.001")
    compareaudio(expected, readaudio())

    expected = concatenate((zeros(96), ref[0:-96]))
    os.system("../file-qdsp -n 64 -i test_in.wav -o test_out.wav -p gain,d=0.002")
    compareaudio(expected, readaudio())


def main():
    test_gain()
    os.remove('test_in.wav')
    os.remove('test_out.wav')


if __name__ == "__main__":
    main()


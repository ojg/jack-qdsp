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
        for i in range(size(data1)):
            print array((data1[i], data2[i]))
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

    expected = minimum(maximum(ref * -0.3, -(10**(-20.0/20))), (10**(-20.0/20)))
    os.system("../file-qdsp -n 64 -i test_in.wav -o test_out.wav -p gain,gl=-0.3,t=-20")
    compareaudio(expected, readaudio())


def main():
    test_gain()
    os.remove('test_in.wav')
    os.remove('test_out.wav')


if __name__ == "__main__":
    main()


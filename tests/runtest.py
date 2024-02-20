#!/usr/bin/env python

from numpy import *
import os as os
import scikits.audiolab as alab

def writeaudio(data):
    alab.wavwrite(data, 'test_in.wav', 48000, 'float32')


def readaudio():
    return alab.wavread("test_out.wav")[0]


def compareaudio(data1, data2, threshold=1e-7):
    if (abs(data1 - data2) > threshold).any():
        print "Fail"
        for i in range(size(data1)):
            print array((i, data1[i], data2[i]))
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


def test_gate():
    print "Testing dsp-gate"

    ref = linspace(-0.25, 0.25, 200)
    writeaudio(ref)

    # test open gate
    expected = ref # * array((zeros(50), ones(100), zeros(50))
    os.system("../file-qdsp -n 64 -i test_in.wav -o test_out.wav -p gate,t=-120")
    compareaudio(expected, readaudio())

    #test closing gate with zero hold
    expected = ref * concatenate((ones(64), linspace(1,0,64), zeros(72)))
    os.system("../file-qdsp -n 64 -i test_in.wav -o test_out.wav -p gate,t=-6")
    compareaudio(expected, readaudio(), 2e-7)

    #test closing gate with hold
    expected = ref * concatenate((ones(64), ones(64), linspace(1,0,64), zeros(8)))
    os.system("../file-qdsp -n 64 -i test_in.wav -o test_out.wav -p gate,t=-6,h=0.003")
    compareaudio(expected, readaudio(), 2e-7)

    #test closing and opening gate with zero hold
    ref = linspace(-0.25, 1, 256)
    writeaudio(ref)
    expected = ref * concatenate((ones(64), linspace(1,0,64), zeros(64), linspace(0,1,64)))
    os.system("../file-qdsp -n 64 -i test_in.wav -o test_out.wav -p gate,t=-6")
    compareaudio(expected, readaudio(), 2e-7)

    #test closing and opening gate with hold
    ref = linspace(-0.25, 1, 384)
    writeaudio(ref)
    expected = ref * concatenate((ones(64), ones(64), linspace(1,0,64), zeros(64), linspace(0,1,64), ones(64)))
    os.system("../file-qdsp -n 64 -i test_in.wav -o test_out.wav -p gate,t=-6,h=0.003")
    compareaudio(expected, readaudio(), 2e-7)


def main():
    ret = test_gain()
    ret = test_gate()
    os.remove('test_in.wav')
    os.remove('test_out.wav')


if __name__ == "__main__":
    main()


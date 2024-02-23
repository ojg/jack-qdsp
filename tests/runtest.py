#!/usr/bin/env python3

from numpy import *
import os as os
import soundfile as sf
from scipy import signal
import sys

def writeaudio(data, filename='test_in.wav'):
    sf.write(file=filename, data=data, samplerate=48000, subtype='FLOAT')


def readaudio(filename='test_out.wav'):
    data, rate = sf.read(file=filename)
    return data


def compareaudio(data1, data2, threshold=1e-7):
    data1 = data1.reshape(data2.shape)
    if (abs(data1 - data2) > threshold).any():
        maxdev = amax(abs(data1 - data2))
        print("Fail %f" % maxdev)
        print(hstack([data1, data2]))
        quit()
    else:
        print("Pass")


def test_gain():
    print("Testing dsp-gain")

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

    expected = minimum(maximum(ref * 2.0, -(10**(0.0/20))), (10**(0.0/20)))
    os.system("../file-qdsp -n 64 -i test_in.wav -o test_out.wav -p gain,gl=2")
    compareaudio(expected, readaudio())


def test_gate():
    print("Testing dsp-gate")

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


def test_iir():
    print("Testing dsp-iir")

    ref = (2.0 * random.rand(512)) - 1.0
    writeaudio(ref)

    #test LP2 mono
    b, a = signal.butter(2, 100.0/24000, 'low')
    expected = signal.lfilter(b,a,ref)
    os.system("../file-qdsp -n 64 -i test_in.wav -o test_out.wav -p iir,lp2,f=100,q=0.7071")
    compareaudio(expected, readaudio(), 1e-6)

    #test HP2 with gain
    b, a = signal.butter(2, 100.0/24000, 'high')
    expected = signal.lfilter(b,a,ref*10**(-6.0/20))
    os.system("../file-qdsp -n 64 -i test_in.wav -o test_out.wav -p iir,hp2,f=100,q=0.7071,g=-6")
    compareaudio(expected, readaudio(), 1e-6)

    #test HP2 stereo
    writeaudio(transpose([ref,-ref]))
    os.system("../file-qdsp -n 64 -i test_in.wav -o test_out.wav -p iir,hp2,f=100,q=0.7071,g=-6")
    compareaudio(transpose([expected, -expected]), readaudio(), 1e-6)

def test_fir():
    print("Testing dsp-fir")

    #ref = ones(512)
    ref = (2.0 * random.rand(512)) - 1.0

    #test short mono fir
    writeaudio(ref)
    h = signal.firwin(21, 0.4)
    savetxt("test_coeffs.txt", h)
    expected = signal.lfilter(h, 1, ref)
    os.system("../file-qdsp -n 64 -i test_in.wav -o test_out.wav -p fir,h=test_coeffs.txt")
    compareaudio(expected, readaudio(), 1e-6)

    #test long mono fir
    writeaudio(ref)
    h = signal.firwin(312, 0.4)
    savetxt("test_coeffs.txt", h)
    expected = signal.lfilter(h, 1, ref)
    os.system("../file-qdsp -n 64 -i test_in.wav -o test_out.wav -p fir,h=test_coeffs.txt")
    compareaudio(expected, readaudio(), 1e-6)

    #test short stereo fir, mono coeffs
    writeaudio(transpose([ref,-ref]))
    h = signal.firwin(21, 0.4)
    savetxt("test_coeffs.txt", h)
    expected = signal.lfilter(h, 1, ref)
    os.system("../file-qdsp -n 64 -i test_in.wav -o test_out.wav -p fir,h=test_coeffs.txt")
    compareaudio(transpose([expected, -expected]), readaudio(), 1e-6)

    #test long stereo fir, mono coeffs
    writeaudio(transpose([ref,-ref]))
    h = signal.firwin(312, 0.4)
    savetxt("test_coeffs.txt", h)
    expected = signal.lfilter(h, 1, ref)
    os.system("../file-qdsp -n 64 -i test_in.wav -o test_out.wav -p fir,h=test_coeffs.txt")
    compareaudio(transpose([expected, -expected]), readaudio(), 1e-6)

    #test asymmetric mono fir
    writeaudio(ref)
    impulse = concatenate(([1], zeros(499)))
    b, a = signal.butter(2, 500.0/24000, 'low')
    h = signal.lfilter(b, a, impulse)
    savetxt("test_coeffs.txt", h)
    expected = signal.lfilter(h, 1, ref)
    os.system("../file-qdsp -n 64 -i test_in.wav -o test_out.wav -p fir,h=test_coeffs.txt")
    compareaudio(expected, readaudio(), 1e-6)

    #test asymmetric stereo fir
    writeaudio(transpose([ref,-ref]))
    impulse = concatenate(([1], zeros(499)))
    b, a = signal.butter(2, 500.0/24000, 'low')
    h = signal.lfilter(b, a, impulse)
    savetxt("test_coeffs.txt", h)
    expected = signal.lfilter(h, 1, ref)
    os.system("../file-qdsp -n 64 -i test_in.wav -o test_out.wav -p fir,h=test_coeffs.txt")
    compareaudio(transpose([expected, -expected]), readaudio(), 1e-6)

    os.remove('test_coeffs.txt')

def test_signal():
    print("Testing dsp-signal")

    #create reference output
    t = linspace(0,1-1/48000,48000)
    expected = sin(2*pi*1000*t)

    #run file-qdsp
    os.system("../file-qdsp -n 64 -i /dev/zero -o test_out.wav -p siggen,sine,f=1000,a=1.0")
    compareaudio(expected, readaudio(), 1e-6)


def benchmarks():
    print("Benchmarking")

    ref = (2.0 * random.rand(131072)) - 1.0
    h = signal.firwin(8191, 0.4)
    expected = signal.lfilter(h, 1, ref)
    savetxt("test_coeffs.txt", h)

    #fir mono benchmark
    writeaudio(ref)
    os.system("../file-qdsp -n 256 -i test_in.wav -o test_out.wav -p fir,h=test_coeffs.txt")
    compareaudio(expected, readaudio(), 1e-5)

    #fir stereo benchmark
    writeaudio(transpose([ref,-ref]))
    os.system("../file-qdsp -n 256 -i test_in.wav -o test_out.wav -p fir,h=test_coeffs.txt")
    compareaudio(transpose([expected, -expected]), readaudio(), 1e-5)

    os.remove('test_coeffs.txt')

    #iir stereo benchmark
    writeaudio(transpose([ref,-ref]))
    b, a = signal.butter(2, 100.0/24000, 'high')
    expected = signal.lfilter(b,a,ref*10**(-6.0/20))
    os.system("../file-qdsp -n 256 -i test_in.wav -o test_out.wav -p iir,hp2,f=100,q=0.7071,g=-6")
    compareaudio(transpose([expected, -expected]), readaudio(), 1e-5)


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "bench":
        benchmarks()
    else:
        test_gain()
        test_gate()
        test_iir()
        test_fir()
#        test_signal()

    os.remove('test_in.wav')
    os.remove('test_out.wav')


if __name__ == "__main__":
    main()


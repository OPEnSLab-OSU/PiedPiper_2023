import os, wave, sys
import numpy as np
import matplotlib.pyplot as plt
from scipy import signal
from scipy.signal import butter, filtfilt

# calibration files
calFile = 'calib.wav'

# input files
inFile = 'PiedPiperInterpolation.wav'
inFile1 = 'ShindoStreamers.wav'

s_freq = 44100

# fundamental frequency of signal, to compute best rms window size
fund_f = 72
# rms window size, based on the period of fundamental frequency
rms_w_s = round(s_freq / fund_f)

# to convert from signed int to float
nb_bits = 2**16

# vibrometer settings: mm/S and peak voltage in Mv
vib_mm = 20
vib_mV = 4000

# calibration measurment: voltage in mV and waveform amplitude
cal_mV = 44
cal_a = 0   # set cal_a to 0 to measure max amplitude from calFile. Otherwise set to peak amplitude of calibration waveform in Audacity *0.0 - 1.0*


# read calFile to determine max amplitude of waveform
if cal_a == 0:
    max = 0
    min = 0
    c = wave.open(calFile, 'r')

    c_len = c.getnframes()

    c_in = []

    for i in range(0, c_len):
        s = c.readframes(1)
        s = int.from_bytes(s, sys.byteorder, signed=True)
        c_in = [s]
        if s > max:
            max = s
        if s < min:
            min = s

    c.close()

    cal_a = (abs(min) + max) / (2.0 * nb_bits)
    #cal_a = max / nb_bits
    print("%0.4f" % cal_a)

# waveform amplitude to mm/s conversion
a_v = (vib_mm * cal_mV) / (cal_a * vib_mV * nb_bits)

def rms(s, s_len):
    #fig, axs = plt.subplots(4, 1)
    out = np.array(s)

    # axs[0].plot(out, label='original')
    # axs[0].legend()

    # square all points
    out = out**2
    # axs[1].plot(out, label='squared')
    # axs[1].legend()

    # average window over time
    rms_w = rms_w_s >> 1
    for i in range(0, s_len):
        # calculate window position
        w_s = 0
        w_e = 0
        if (i - rms_w < 0):
            w_s = 0
        else:
            w_s = i - rms_w

        if (i + rms_w > s_len):
            w_e = s_len
        else:
            w_e = i + rms_w
        # find average of window
        out[i] = np.mean(out[w_s:w_e])
    # axs[2].plot(out, label='windowed')
    # axs[2].legend()
    
    out = out**(0.5)

    # axs[3].plot(out, label="rms")
    # axs[3].legend()

    # plt.show()
    return out

# read inFile and convert waveform amplitude to mm/S
w = wave.open(inFile, 'r')
w1 = wave.open(inFile1, 'r')

w_len = w.getnframes()
w1_len = w1.getnframes()

# stores values
s_in = []
s1_in = []
# the time domain of inFile
time = np.linspace(0, w_len / s_freq, w_len)
time1 = np.linspace(0, w1_len / s_freq, w1_len)

# read from w and find max
s_max = 0
s_max_idx = 0 
for i in range(0, w_len):
    s = w.readframes(1)
    s = int.from_bytes(s, sys.byteorder, signed=True)
    s *= a_v
    s_in.append(s)
    if s_max < s:
        s_max = s
        s_max_idx = i
    

w.close()

# read from w1 and find max
s1_max = 0
s1_max_idx = 0
for i in range(0, w1_len):
    s = w1.readframes(1)
    s = int.from_bytes(s, sys.byteorder, signed=True)
    s *= a_v
    s1_in.append(s)
    if s1_max < s:
        s1_max = s
        s1_max_idx = i

w1.close()

s_rms = rms(s_in, w_len)
s1_rms = rms(s1_in, w1_len)

plt.plot(time, s_rms, label=inFile)
plt.ylabel("RMS (mm/S)")
plt.xlabel("Time (S)")
plt.legend()
plt.show()

plt.plot(time1, s1_rms, label=inFile1)
plt.ylabel("RMS (mm/S)")
plt.xlabel("Time (S)")
plt.legend()
plt.show()

# s_in = butter_highpass_filter(s_in, 20, s_freq)

# grab values within 5ms of max (in both directions)
ten_ms_o = round(s_freq * .005)
s_in_ten_ms = s_in[s_max_idx - ten_ms_o : s_max_idx + ten_ms_o]
s1_in_ten_ms = s1_in[s1_max_idx - ten_ms_o : s1_max_idx + ten_ms_o]

time_10ms = np.linspace(0, len(s_in_ten_ms) / s_freq, len(s_in_ten_ms))

s_in_mean = np.mean(s_in_ten_ms)
# print(np.max(s_in_ten_ms))
# print(np.max(s1_in_ten_ms))
#print(s_in_mean)

# plot data
#plt.plot(time, s_in)
#plt.plot(time1, s1_in)

plt.plot(time_10ms, s_in_ten_ms, label = inFile)
plt.plot(time_10ms, s1_in_ten_ms, label = inFile1)
plt.ylabel("Velocity (mm/S)")
plt.xlabel("Time (S)")
plt.legend()
plt.show()
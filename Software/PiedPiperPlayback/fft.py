import numpy as np
import matplotlib.pyplot as plt

sample_rate = 8192
window_size = 256

def combine_waves(freqs):
    signal = 0.0
    for f, a in freqs:
        print("freq: %d, amp %d" % (f, a))
        signal += a * np.sin(f * _sample_rate * s_time)
    return signal

window_time = window_size / sample_rate
_sample_rate = 2 * np.pi / sample_rate


r_time = np.linspace(0.0, window_time, window_size)
s_time = np.arange(window_size)

# signal freqs and amplitudes (freq, amp)
signal_freqs_amps = [(100, 127), (150, 100), (400, 20), (1500, 30), (3000, 50)]

signal = combine_waves(signal_freqs_amps)

# plot signal

plt.plot(r_time, signal)
plt.show()

# plot fft

signal_fft = np.fft.fft(signal)

signal_mean_sub = np.abs(signal_fft - np.mean(signal_fft))

signal_power_spectrum = np.abs(signal_fft)

freqs = signal_power_spectrum[0:int(window_size / 2) + 1]

freqs2 = signal_mean_sub[0:int(window_size / 2) + 1]

fft_bins = np.linspace(0.0, sample_rate / 2, int(window_size / 2) + 1)

fig, axs = plt.subplots(2, 1)

plt.plot(fft_bins, abs(freqs))

plt.plot(fft_bins, abs(freqs2))

plt.show()

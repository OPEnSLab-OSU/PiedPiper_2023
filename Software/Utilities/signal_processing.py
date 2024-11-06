import wave, sys
import numpy as np
import matplotlib.pyplot as plt
from matplotlib import cm
import scipy.io.wavfile as wf

### SIGNAL GENERATOR ###    

def generateTone(sampleRate, windowSize, frequency, amplitude, phase):

    output = []
    step = 2.0 * np.pi * frequency / sampleRate
    for i in range(0, windowSize):
        output.append(amplitude * np.sin(step * i + phase))

    return np.array(output)

### SINC FILTER ###

def downsampleTable(ratio, nz):
    """
    downsampleTable generate sinc function table for downsampling
    : param: ratio: downsample ratio
    : param: nz: num zero crossings for sinc filter
    : return: sinc function table
    """
    n = (2*nz + 1) * ratio - ratio + 1
    ns = np.linspace(-nz * ratio, nz * ratio, n)

    ns[round((n - 1) / 2)] = 1.0

    sTable = (1/ratio) * np.sin(np.pi * ns / ratio) / (np.pi * ns / ratio)

    sTable[round((len(sTable) - 1) / 2)] = 1 / ratio

    t = np.linspace(0,1,n)

    sTable = sTable * 0.5 * (1 - np.cos(2*np.pi*t))

    return sTable, len(sTable)

def upsampleTable(ratio, nz):
    """
    upsampleTable generate sinc function table for upsampling
    : param: ratio: downsample ratio
    : param: nz: num zero crossings for sinc filter
    : return: sinc function table
    """
    n = (2*nz + 1) * ratio - ratio + 1
    ns = np.linspace(-nz, nz, n)
    
    ns[round((n - 1) / 2)] = 1.0

    sTable = np.sin(np.pi * ns) / (np.pi * ns)
    
    sTable[round((len(sTable) - 1) / 2)] = 1.0

    t = np.linspace(0,1,n)

    sTable = sTable * 0.5 * (1 - np.cos(2*np.pi*t))

    return sTable, len(sTable)

def downsampleSignal(signal, ratio, nz):
    """
    downsampleSignal downsample a signal by ratio
    : param: signal: some signal (1d array)
    : param: ratio: downsample ratio
    : param: nz: num zero crossings for sinc filter
    : return: downsampled signal
    """
    table, tableSize = downsampleTable(ratio, nz)

    filterInput = [0] * tableSize

    signalLen = len(signal)
    outputLen = int(round(signalLen / ratio))
    filterOutput = [0] * int(round(signalLen / ratio))
    outputIdx = 0
    
    signalIdx = 0
    tableIdx = 0

    while 1:
        tableIdxCpy = tableIdx
        for i in range(ratio):
            if (signalIdx < signalLen):
                filterInput[tableIdx] = signal[signalIdx]
            else:
                filterInput[tableIdx] = 0
            tableIdx += 1
            signalIdx += 1
            if tableIdx == tableSize: tableIdx = 0
        
        downsampledValue = 0
        for i in range(tableSize):
            downsampledValue += filterInput[(tableIdxCpy + i) % tableSize] * table[i]

        if outputIdx >= outputLen: break
        filterOutput[outputIdx] = downsampledValue
        outputIdx += 1

        if (signalIdx >= signalLen):
            break
    
    return np.array(filterOutput)

def upsampleSignal(signal, ratio, nz):
    """
    upsampleSignal upsample a signal by ratio
    : param: signal: some signal (1d array)
    : param: ratio: upsample ratio
    : param: nz: num zero crossings for sinc filter
    : return: upsampled signal
    """
    table, tableSize = upsampleTable(ratio, nz)

    filterInput = [0] * tableSize

    signalLen = len(signal)
    filterOutput = [0] * signalLen * ratio
    outputIdx = 0
    
    signalIdx = 0
    tableIdx = 0

    upsampleCount = 0

    while 1:
        tableIdxCpy = tableIdx
        if (upsampleCount == 0):
            filterInput[tableIdx] = signal[signalIdx]
            signalIdx += 1
        else:
            filterInput[tableIdx] = 0
        upsampleCount += 1
        tableIdx += 1
        if upsampleCount == ratio: upsampleCount = 0
        if tableIdx == tableSize: tableIdx = 0

        upsampledValue = 0
        for i in range(tableSize):
            upsampledValue += filterInput[(tableIdxCpy + i) % tableSize] * table[i]
        
        filterOutput[outputIdx] = upsampledValue
        outputIdx += 1

        if (signalIdx >= signalLen):
            break

    return np.array(filterOutput)

### INTERPOLATION ###

def interpolateSignal(signal, interpRatio):
    """
    interpolateSignal interpolates some signal by ratio
    : param: signal: some real signal (1d array)
    : param: interpRatio: interpolation ratio
    : return: interpolated signal
    """
    interpCount = 0

    interpCoeffA = 0
    interpCoeffB = 0
    interpCoeffC = 0
    interpCoeffD = 0

    signalLen = len(signal)
    signalIdx = 0

    output = []
    
    while signalIdx < (signalLen - 3):

        interpCount += 1
        if not (interpCount < interpRatio):
            interpCount = 0
            signalIdx += 1

            interpCoeffA = -0.5 * signal[signalIdx - 1] + 1.5 * signal[signalIdx] - 1.5 * signal[signalIdx + 1] + 0.5 * signal[signalIdx + 2]
            interpCoeffB = signal[signalIdx - 1] - 2.5 * signal[signalIdx] + 2 * signal[signalIdx + 1] - 0.5 * signal[signalIdx + 2]
            interpCoeffC = -0.5 * signal[signalIdx - 1] + 0.5 * signal[signalIdx + 1]
            interpCoeffD = signal[signalIdx]

        t = interpCount / interpRatio
        nextSample = interpCoeffA * t * t * t + interpCoeffB * t * t + interpCoeffC * t + interpCoeffD

        output.append(nextSample)

    return np.array(output)


### RC Filter ###

def RCFilter(signal, sampleRate, cutoffFrequency):
    """
    RCFilter runs some signal through RC filter
    : param: signal: some real signal (1d array)
    : param: sampleRate: sample rate of signal
    : param: cutoffFrequency: cutoff frequency to use
    : return: filtered signal
    """

    filter_k =  sampleRate / (np.pi * cutoffFrequency)

    output = [0] * 2

    for t in range(1, len(signal) - 1):
        filteredValue = (signal[t + 1] - signal[t - 1] + (2 * filter_k * output[t]) + (1 - filter_k) * signal[t - 1]) / (filter_k + 1)
        output.append(filteredValue)

    return output

# def RCFilterUpsampling(signal, sampleRate, cutoffFrequency, ratio):

#     filter_k =  sampleRate / (np.pi * cutoffFrequency)

#     output = [0] * 2

#     ratioCount = 0
#     for t in range(1, len(signal) - 1):
#         for i in range(ratio):
#             filteredValue = 0
#             if i > 0:
#                 filteredValue = (signal[t + 1] - signal[t - 1] + (2 * filter_k * output[t]) + (1 - filter_k) * signal[t - 1]) / (filter_k + 1)
#             else:
#                 filteredValue = (signal[t + 1] - signal[t - 1] + (2 * filter_k * output[t]) + (1 - filter_k) * signal[t - 1]) / (filter_k + 1)
#             output.append(filteredValue)

#     return output

### PRINT AND SAVE SIGNALS ###

TDSignalSaveCount = 0
FDSignalSaveCount = 0

def printTimeDomainSignal(signal, sampleRate, title=None, ylabel=None, show=True, save=False, fname=None):
    """
    printTimeDomainSignal plots a signal
    : param: signal: some TD signal (1d array)
    : param: sampleRate: sample rate of signal
    : param: title: title of plot
    : param: ylabel: y-axis label
    : param: show: show the plot (default True)
    : param: save: save the plot (default False)
    : param: fname: output file name, will save file as 'signal#.png' if unspecified (default None)
    """
    time = np.linspace(0, len(signal) / sampleRate, len(signal))

    plt.plot(time, signal)
    plt.xlabel("Time (seconds) - %d samples" % (len(signal)))
    if ylabel != None: 
        plt.ylabel(ylabel)
    plt.title("%s - %dHz sample rate" % (title, sampleRate))

    if save:
        if fname == None:
            global TDSignalSaveCount
            plt.savefig("TD_signal%d" % (TDSignalSaveCount))
            TDSignalSaveCount += 1
        else:
            plt.savefig(fname)
    
    if show:
        plt.show()

def loadTimeDomainSignal(inFile):
    """
    loadTimeDomainSignal loads some audio data (.wav file exported as signed 16 bit recommended)
    : param: inFile: input filename
    : return: samples of audio data (1d array)
    """
    w = wave.open(inFile, 'r')
    w_len = w.getnframes()

    w_signal = []
    for i in range(w_len):
        s = w.readframes(1)
        s = int.from_bytes(s, sys.byteorder, signed=True)
        w_signal.append(s)
    w.close()

    return np.array(w_signal)

def saveTimeDomainSignal(outFile, sampleRate, signal):
    """
    saveTimeDomainSignal saves some signal as audio
    : param: outFile: output filename
    : param: srate: sample rate of signal
    : param: signal: some signal (1d array)
    """
    wf.write(outFile, sampleRate, np.int16(signal))

def printFrequencyDomainSignal(signal, sampleRate, title=None, ylabel=None, show=True, save=False, fname=None):
    """
    printTimeDomainSignal plots a signal
    : param: signal: some FD signal (1d array)
    : param: sampleRate: sample rate of signal
    : param: title: title of plot
    : param: ylabel: y-axis label
    : param: show: show the plot (default True)
    : param: save: save the plot (default False)
    : param: fname: output file name, will save file as 'FD_signal#.png' (i.e. FD_signal0.png) if unspecified (default None)
    """

    frequency = np.arange(0, sampleRate >> 1, sampleRate / (2 * len(signal)))

    plt.plot(frequency, signal)
    plt.xlabel("Frequency (Hz) - %d bins" % (len(signal)))
    if ylabel != None: 
        plt.ylabel(ylabel)
    plt.title("%s - %dHz sample rate" % (title, sampleRate))

    if save:
        if fname == None:
            global FDSignalSaveCount
            plt.savefig("FD_Signal%d" % (FDSignalSaveCount))
            FDSignalSaveCount += 1
        else:
            plt.savefig(fname)
    
    if show:
        plt.show()
    else:
        plt.close()


def saveData(data, filename):
    """
    saveData saves data from array (1d or 2d) into txt file
    : param: data: specgram data (1d or 2d array)
    : param: filename: output file name (.txt)
    """

    with open(filename, "w") as file:
        for window in data:
            for m in window:
                file.write(str(m) + '\n')

### SPECTROGRAM ###

def performFFT(data):
    """
    performFFT computes FFT of some data (window size = len(data))
    : param: data: some signal (1d array)
    : return: FFT of data
    """
    _windowSize = len(data)
    _fft = np.fft.fft(data)
    return abs(_fft[:_windowSize >> 1])


def dataToSpecgram(data, windowSize):
    """
    dataToSpecgram generates specgram from a real signal
    : param: data: some signal (1d array)
    : param: windowSize: windowSize to use for specgram 
    : return: specgram of data
    """
    _dataLen = len(data)
    _dataRemainder = (_dataLen) % windowSize
    _inData = np.array_split(data[0:_dataLen - _dataRemainder], int(_dataLen / windowSize))

    _outData = []

    for _data in _inData:
        _outData.append(performFFT(_data))

    return np.array(_outData)

def specgramCutoff(data, windowSizeCutoff):
    """
    specgramCutoff crops frequency data of a specgram
    : param: data: specgram data (2d array)
    : param: windowSizeCutoff: crop specgram to this data
    : return: cropped specgram
    """
    if (len(data[0]) < windowSizeCutoff):
        print("data window must be greater than window cutoff")
        return []
    _outData = []
    for d in data:
        _outData.append(d[:windowSizeCutoff])

    return np.array(_outData)

def spectralSubtraction(data1, data2):
    """
    spectralSubtration subtacts 2 specgrams abs(data1 - data2)
    : param: data1: specgram data (2d array)
    : param: data2: specgram data (2d array)
    : return: spectral subtraction specgram between data1 and data2
    """
    data1_windowSize = len(data1[0])
    data2_windowSize = len(data2[0])
    
    data1 /= np.sum(data1)
    data2 /= np.sum(data2)

    if data1_windowSize > data2_windowSize:
        data1 = specgramCutoff(data1, data2_windowSize)
    elif data2_windowSize > data1_windowSize:
        data2 = specgramCutoff(data2, data1_windowSize)

    return abs(data1 - data2)

def spectralError(data1, data2):
    """
    spectralError computes spectral error of 2 specgrams
    : param: data1: specgram data (2d array)
    : param: data2: specgram data (2d array)
    : return: spectral error specgram between data1 and data2
    """
    data1_windowSize = len(data1[0])
    data2_windowSize = len(data2[0])

    # normalize data
    data1 /= np.sum(data1)
    data2 /= np.sum(data2)

    # ensure window sizes match    
    if data1_windowSize > data2_windowSize:
        data1 = specgramCutoff(data1, data2_windowSize)
    elif data2_windowSize > data1_windowSize:
        data2 = specgramCutoff(data2, data1_windowSize)

    return (abs(data2 - data1) / data1) * 100

def crossCorrelation(data1, data2):
    """
    crossCorrelation computes correlation coeffiecent of data1 and data2
    : param: data1: specgram data (2d array)
    : param: data2: specgram data (2d array)
    : return: correlation coeffiecent
    """
    data1_windowSize = len(data1[0])
    data2_windowSize = len(data2[0])    

    # ensure window sizes match    
    if data1_windowSize > data2_windowSize:
        data1 = specgramCutoff(data1, data2_windowSize)
    elif data2_windowSize > data1_windowSize:
        data2 = specgramCutoff(data2, data1_windowSize)

    # do dot product, dividing by sum of data1 and data2 to normalize
    
    dot_data1_data2 = data1 * data2

    crossProduct = np.sqrt(np.sum(data1 * data1)) * np.sqrt(np.sum(data2 * data2))
    
    dot_data1_data2 /= crossProduct

    return dot_data1_data2

# align data2 to data1 based on the maximum value in the data and their sample rate
def signalAlignment(data1, data2, data1SampleRate, data2SampleRate):
    """
    signalAlignment aligns data2 to data1 based on the maximum value (used for aligning signals for more accurate correlation)
    : param: data1: real signal (data to align to)
    : param: data2: real signal (data to align)
    : param: data1SampleRate: sample rate of data 1
    : param: data2SampleRate: sample rate of data 2
    : return: aligned data 2
    """
    # find maximum index in both signals
    output = []

    dataMax = 0
    data1MaxIdx = 0
    for i in range(0, len(data1)):
        if (data1[i] > dataMax):
            dataMax = data1[i]
            data1MaxIdx = i
    
    dataMax = 0
    data2MaxIdx = 0
    for i in range(0, len(data2)):
        if (data2[i] > dataMax):
            dataMax = data2[i]
            data2MaxIdx = i

    # convert index to time
    data1MaxTime = data1MaxIdx / data1SampleRate
    data2MaxTime = data2MaxIdx / data2SampleRate

    # get the time difference
    timeDifference = abs(data1MaxTime - data2MaxTime)

    timeDifferenceToIndex = int(round(timeDifference * data2SampleRate))
    
    # pad aligned signal with zeroes
    if data1MaxTime > data2MaxTime:
        output = np.concatenate(([0] * timeDifferenceToIndex, data2[0:len(data2) - timeDifferenceToIndex]))
    else:
        output = np.concatenate((data2[timeDifferenceToIndex:], [0] * timeDifferenceToIndex))

    return output

# global variable for saving figures
specSaveCount = 0

def printSpecgram(data, sampleRate, windowSize = None, startFreq = None, endFreq = None, min = None, max = None, title = None, shading='gouraud', show=True, save=False, fname=None):
    """
    printSpecgram plots 2d specgram using pcolormesh
    : param: data: some specgram data (2d array)
    : param: sampleRate: sample rate of specgram data
    : param: windowSize: window size of specgram (not necassary if specgram is uncropped)
    : param: startFreq: custom frequency scale min (default None)
    : param: endFreq: custom frequency scale max (default None)
    : param: min: min value for specgram (default None)
    : param: max: max value for specgram (default None)
    : param: title: title of plot (default None)
    : param: shading: shading to use for specgram (default 'gourand')
    : param: show: show the plot (default True)
    : param: save: save the plot (default False)
    : param: fname: output file name, will save file as 'specgram#.png' if unspecified (default None)
    """
    spec_out = []


    for i in range(0, len(data[0])):
        spec_out.append([])
        for j in range(0, len(data)):
            spec_out[i].append(data[j][i])

    nyquist = int(sampleRate / 2)

    wt = len(data[0]) / nyquist
    if (windowSize != None):
        wt = windowSize / nyquist / 2

    td = np.linspace(0, len(data) * wt, len(data))
    fd = np.linspace(0, nyquist, len(data[0]))
    if (startFreq != None and endFreq != None):
        fd = np.linspace(startFreq, endFreq, len(data[0]))

    plt.pcolormesh(td, fd, spec_out, shading=shading, cmap=plt.colormaps['plasma'], vmin=min, vmax=max)
    plt.xlabel("Time (seconds)")
    plt.ylabel("Frequency (Hz)")
    plt.title(title)

    if save:
        if fname == None:
            global specSaveCount
            plt.savefig("specgram%d" % (specSaveCount))
            specSaveCount += 1
        else:
            plt.savefig(fname)
    
    if show:
        plt.show()

    plt.close()

def printSpecgramSurfacePlot(data, sampleRate, zlabel = None, max = None, title = None):
    """
    printSpecgramSurfacePlot plots 3d specgram using surface plot
    : param data: some specgram data (2d array)
    : param sampleRate: sample rate of specgram data
    : param: zlabel: label for z-axis
    : param: max: crop data to this value
    : param: title: title of surface plot
    """
    wt = len(data[0]) / (sampleRate >> 1)   

    X = np.linspace(0, sampleRate >> 1, len(data[0]))
    Y = np.linspace(0, len(data) * wt, len(data))
    X, Y = np.meshgrid(X, Y)
    Z = data

    fig, ax = plt.subplots(subplot_kw={"projection": "3d"})
    ax.plot_surface(X, Y, Z, vmin=0, vmax=max, cmap=cm.plasma)

    ax.invert_xaxis()

    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Time (seconds)")
    ax.set_zlabel(zlabel)

    plt.title(title)

    plt.show()

### noise subtraction ###

def smoothFreqs(data, winSize):
    """
    averageSpecgram computes moving average of some data based on smoothing window size
    : param data: some data (1d array)
    : param winSize: number of samples around current sample to use for computing average
    : return: smoothed data
    """
    dataLen = len(data)

    output = [0] * dataLen

    # get moving average of each sample [i - margin : i + margin]
    for i in range(dataLen):
        startIdx = int(max(0, i - winSize / 2))
        endIdx = int(min(dataLen - 1, i + winSize / 2))
        
        tempData = data[startIdx:(endIdx + 1)]
        output[i] = np.average(tempData)
    
    return output


def alphaTrimming(data, winSize, threshold):
    """
    alphaTrimming removes peaks which deviation is below threshold from a window of FFT data
    : param data: some data (1d array)
    : param winSize: number of samples around current sample to use for computing standard deviation and mean
    : param threshold: sample deviation threshold, samples below this threshold will be subtracted
    : return: trimmed data
    """

    dataLen = len(data)

    tempData = np.copy(data)

    # for each sample in data, compute standard devition of sample in current window
    for i in range(dataLen):
        # window bounds
        startIdx = int(max(0, i - winSize / 2))
        endIdx = int(min(dataLen - 1, i + winSize / 2))
        # window mean and standard deviation
        marginAvg = np.average(data[startIdx:(endIdx + 1)])
        marginStdDev = np.std(data[startIdx:(endIdx + 1)])

        # for each sample within bounds [startIdx:endIdx], check if deviation of sample in current window is above some threshold
        for k in range(startIdx, endIdx + 1):
            # if sample deviation is above threshold, exclude sample from trimming data
            if ((data[k] - marginAvg) / marginStdDev) > threshold:
                # replace sample in tempData with average value of samples in window (excluding this sample)
                tempData[k] = np.average(np.hstack((data[startIdx:k], data[k + 1:endIdx + 1])))

    # trimming
    return np.array(data) - np.array(tempData)


def averageSpecgram(data):
    """
    averageSpecgram averages windows of a specgram
    : param data: specgram data
    : return: averaged window
    """
    return np.average(data, axis=0)

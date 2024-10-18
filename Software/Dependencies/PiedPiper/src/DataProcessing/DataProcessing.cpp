#include "DataProcessing.h"

ArduinoFFT fft = ArduinoFFT();

void FFT(float *inputReal, float *inputImag, uint16_t windowSize) { 
    fft.DCRemoval(inputReal, windowSize);
    fft.Windowing(inputReal, windowSize, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    fft.Compute(inputReal, inputImag, windowSize, FFT_FORWARD);
    fft.ComplexToMagnitude(inputReal, inputImag, windowSize);
}

template <typename T> void TimeSmoothing(T **input, T *output, uint16_t windowSize, uint16_t numWindows) {
    T sum = 0;
    float _numWindows = 1.0 / numWindows;
    for (uint16_t f = 0; f < windowSize >> 1; f++) {
        sum = 0;
        output[f] = 0;
        for (uint16_t t = 0; t < numWindows; t++) {
            sum += input[t][f];
        }
        output[f] = sum * _numWindows;
    }    
}

template <typename T> void FrequencySmoothing(T *input, T *output, uint16_t windowSize, uint16_t smoothingSize) {
    uint16_t startIdx, endIdx;
    for (uint16_t f = 0; f < windwSize >> 1; f++) {
        startIdx = max(0, f - smoothingSize);
        endIdx = min((windowSize >> 1) - 1, f + smoothingSize)
        output[f] = 0;
        for (uint16_t s = startIdx; s <= endIdx; s++) {
            output[f] += input[s];
        }
        output[f] /= endIdx - startIdx;
    }
}

template <typename T> void AlphaTrimming(T *input, T *output, uint16_t windowSize, uint16_t smoothingSize, float deviationThreshold) {

    // copy data to output array to use as scratchpad array
    for (uint16_t i = 0; i < windowSize >> 1; i++) {
        output[i] = input[i];
    }
    
    uint16_t startIdx, endIdx, boundNumSamples;

    float boundSum, boundAvg, boundStdDev;

    for (uint16_t i = 0; i < windowSize >> 1; i++) {
        // calculate lower and upper bounds based on smoothingSize
        startIdx = max(0, f - smoothingSize);
        endIdx = min((windowSize >> 1) - 1, f + smoothingSize);
        boundNumSamples = endIdx - startIdx;

        // get average of magnitudes within lower and upper bound
        boundSum = 0;
        for (uint16_t s = startIdx; s <= endIdx; s++) {
            boundSum += input[s];
        }
        boundAvg = boundSum / boundNumSamples;

        // get standard deviation of magnitudes within lower and upper bound
        boundStdDev = 0;
        for (uint16_t s = startIdx; s <= endIdx; s++) {
            boundStdDev += pow(input[s] - boundAvg, 2);
        }
        boundStdDev = sqrt(boundStdDev / boundNumSamples);

        // check deviation of each sample within lower and upper bound. If sample deviation is greater than some threshold, 
        // replace sample in subtraction data with the average of the bound excluding this sample
        boundNumSamples -= 1;
        for (uint16_t s = startIdx; s <= endIdx; s++) {
            if ((input[s] - boundAvg) / boundStdDev > deviationThreshold)
                output[s] = (boundSum - input[s]) / boundNumSamples;
        }
    }

    // subtraction of trimmed data from raw data
    for (uint16_t i = 0; i < windowSize >> 1; i++) {
        output[i] = input[i] - output[i];
    }

}

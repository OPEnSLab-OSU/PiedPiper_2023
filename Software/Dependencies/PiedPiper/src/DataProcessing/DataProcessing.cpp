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
    for (uint16_t i = 0; i < windowSize >> 1; i++) {
        output[i] = input[i];
    }

    uint16_t startIdx, endIdx, boundAvgCount;

    float boundAvg, boundStdDev, trimmedAvg;

    for (uint16_t i = 0; i < windowSize >> 1; i++) {
        startIdx = max(0, f - smoothingSize);
        endIdx = min((windowSize >> 1) - 1, f + smoothingSize);
        boundAvgCount = endIdx - startIdx;

        // get average of magnitudes within lower and upper bound
        boundAvg = 0;
        for (uint16_t s = startIdx; s <= endIdx; s++) {
            boundAvg += input[s];
        }
        boundAvg /= boundAvgCount;

        // get standard deviation of magnitudes within lower and upper bound
        boundStdDev = 0;
        for (uint16_t s = startIdx; s <= endIdx; s++) {
            boundStdDev += pow(input[s] - boundAvg, 2);
        }

        boundStdDev = sqrt(boundStdDev / boundAvgCount);

        // check deviation of samples within lower and upper bound
        for (uint16_t s = startIdx; s <= endIdx; s++) {
            if ((input[s] - boundAvg) / boundStdDev > deviationThreshold) {
                // replace this sample in subtraction data with the average excluding this sample
                trimmedAvg = 0;
                for (uint16_t j = startIdx; j <= endIdx; j++) {
                    if (j == s) continue;
                    trimmedAvg += input[j];
                }
                output[s] = trimmedAvg / (boundAvgCount - 1);
            }
        }
    }

    // subtraction step
    for (uint16_t i = 0; i < windowSize >> 1; i++) {
        output[i] = input[i] - output[i];
    }

}

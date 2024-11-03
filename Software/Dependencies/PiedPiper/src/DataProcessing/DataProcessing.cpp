#include "DataProcessing.h"

ArduinoFFT<float> fft = ArduinoFFT<float>();

void FFT(float *inputReal, float *inputImag, uint16_t windowSize) { 
    fft.dcRemoval(inputReal, windowSize);
    fft.windowing(inputReal, windowSize, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    fft.compute(inputReal, inputImag, windowSize, FFT_FORWARD);
    fft.complexToMagnitude(inputReal, inputImag, windowSize);
}

// template <typename T>
// void TimeSmoothing(T **input, T *output, uint16_t windowSize, uint16_t numWindows) {
    
//     T sum = 0;
//     uint16_t freq, time;

//     float _numWindows = 1.0 / numWindows;
//     for (freq = 0; freq < windowSize >> 1; freq++) {
//         sum = 0;
//         output[freq] = 0;
//         for (time = 0; time < numWindows; time++) {
//             sum += input[time][freq];
//         }
//         output[freq] = sum * _numWindows;
//     }    
// }

// template <typename T>
// void FrequencySmoothing(T *input, T *output, uint16_t windowSize, uint16_t smoothingSize) {

//     uint16_t freq, samp, startIdx, endIdx;

//     for (freq = 0; freq < windowSize >> 1; freq++) {
//         startIdx = max(0, freq - smoothingSize);
//         endIdx = min((windowSize >> 1) - 1, freq + smoothingSize);
//         output[freq] = 0;
//         for (samp = startIdx; samp <= endIdx; samp++) {
//             output[freq] += input[samp];
//         }
//         output[freq] /= endIdx - startIdx;
//     }
// }

// template <typename T>
// void AlphaTrimming(T *input, T *output, uint16_t windowSize, uint16_t smoothingSize, float deviationThreshold) {

//     uint16_t i, s, startIdx, endIdx, boundNumSamples;
//     float boundSum, boundAvg, boundStdDev;

//     // copy data to output array to use as scratchpad array
//     for (i = 0; i < windowSize >> 1; i++) {
//         output[i] = input[i];
//     }

//     for (i = 0; i < windowSize >> 1; i++) {
//         // calculate lower and upper bounds based on smoothingSize
//         startIdx = max(0, i - smoothingSize);
//         endIdx = min((windowSize >> 1) - 1, i + smoothingSize);
//         boundNumSamples = endIdx - startIdx;

//         // get average of magnitudes within lower and upper bound
//         boundSum = 0;
//         for (s = startIdx; s <= endIdx; s++) {
//             boundSum += input[s];
//         }
//         boundAvg = boundSum / boundNumSamples;

//         // get standard deviation of magnitudes within lower and upper bound
//         boundStdDev = 0;
//         for (s = startIdx; s <= endIdx; s++) {
//             boundStdDev += pow(input[s] - boundAvg, 2);
//         }
//         boundStdDev = sqrt(boundStdDev / boundNumSamples);

//         // check deviation of each sample within lower and upper bound. If sample deviation is greater than some threshold, 
//         // replace sample in subtraction data with the average of the bound excluding this sample
//         boundNumSamples -= 1;
//         for (s = startIdx; s <= endIdx; s++) {
//             if ((input[s] - boundAvg) / boundStdDev > deviationThreshold)
//                 output[s] = (boundSum - input[s]) / boundNumSamples;
//         }
//     }

//     // subtraction of trimmed data from raw data
//     for (i = 0; i < windowSize >> 1; i++) {
//         output[i] = input[i] - output[i];
//     }

// }

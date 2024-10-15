#include "DataProcessing.h"

ArduinoFFT fft = ArduinoFFT();

void FFT(float *inputReal, float *inputImag, uint16_t windowSize) { 
    fft.DCRemoval(inputReal, windowSize);
    fft.Windowing(inputReal, windowSize, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    fft.Compute(inputReal, inputImag, windowSize, FFT_FORWARD);
    fft.ComplexToMagnitude(inputReal, inputImag, windowSize);
}

template <typename T> void TimeSmoothing(T **input, T *output, uint16_t windowSize, uint16_t smoothingSize) {
    
}

template <typename T> void FrequencySmoothing(T *input, T *output, uint16_t windowSize, uint16_t smoothingSize) {

}

template <typename T> void AlphaTrimming(T *input, T *output, uint16_t windowSize, uint16_t smoothing, float deviationThreshold) {

}

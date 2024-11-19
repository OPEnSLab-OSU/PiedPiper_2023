#include "DataProcessing.h"

ArduinoFFT<float> fft = ArduinoFFT<float>();

void FFT(float *inputReal, float *inputImag, uint16_t windowSize) { 
    fft.dcRemoval(inputReal, windowSize);
    fft.windowing(inputReal, windowSize, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    fft.compute(inputReal, inputImag, windowSize, FFT_FORWARD);
    fft.complexToMagnitude(inputReal, inputImag, windowSize);
}

void iFFT(float *inputReal, float *inputImag, uint16_t windowSize) { 
    fft.compute(inputReal, inputImag, windowSize, FFT_REVERSE);
    // fft.complexToMagnitude(inputReal, inputImag, windowSize);
    fft.windowing(inputReal, windowSize, FFT_WIN_TYP_HAMMING, FFT_REVERSE);
}
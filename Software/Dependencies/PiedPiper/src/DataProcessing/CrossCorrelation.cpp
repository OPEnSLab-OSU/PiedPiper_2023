#include "DataProcessing.h"

class CrossCorrelation::CrossCorrelation(uint16_t sampleRate, uint16_t windowSize) {
    this->templatePtr = NULL;

    this->templateSqrtSum = 0;
    this->numRows = 0;
    this->numCols = 0;

    this->sampleRate = sampleRate;
    this->windowSize = windowSize;
    this->frequencyIndexLow = 0;
    this->frequencyIndexHigh = (sampleRate >> 1) * (float(windowSize) / sampleRate);
}

void CrossCorrelation::computeTemplate() {
    this->templateSqrtSumSq = 0;

    uint32_t _sumSq = 0;
    uint32_t _templateValue = 0;

    // computing square root sum squared of template
    for (uint16_t t = 0; t < numCols; t++) {
        for (uint16_t f = this->frequencyIndexLow; f < this->frequencyIndexHigh; f++) {
            _templateValue = *((this->bufferPtr + f) + t * this->numRows);
            _sumSq += templateValue * templateValue;
        }
    }
    this->templateSqrtSumSq = sqrt(_sumSq);
}


void CrossCorrelation::setTemplate(uint16_t *input, uint16_t numRows, uint16_t numCols, uint16_t frequencyRangeLow, uint16_t frequencyRangeHigh) {
    this->templatePtr = input;
    this->numRows = numRows;
    this->numCols = numCols;

    float _frequencyWidth = float(windowSize) / sampleRate
    this->frequencyStartIndex = round(frequencyRangeLow * _frequencyWidth);
    this->frequencyEndIndex = round(frequencyRangeHigh * _frequencyWidth);

    this->computeTemplate();
}

float CrossCorrelation::correlate(uint16_t *inputPtr, uint16_t inputStartWindowIndex, uint16_t inputTotalWindows = this->numCols) {
    if (templatePtr == NULL) return 0;

    uint32_t _inputSqrtSumSq = 0;
    uint32_t _inputValue = 0;

    // cross correlation introduces a delay depending on the length of template, to solve this...
    // subtract length of template from current time index in input
    uint16_t _inputWindowIndex = (inputStartWindowIndex - this->numCols + inputTotalWindows) % inputTotalWindows;
    uint16_t _tempInputWindowIndex = _inputWindowIndex;

    // computing square root sum squared of input
    for (uint16_t t = 0; t < this->numCols; t++) {
        for (uint16_t f = this->frequencyStartIndex; f < this->frequencyEndIndex; f++) {
            _inputValue = *((this->inputPtr + f) + _tempWindowIndex++ * this->numRows);
            _inputSqrtSumSq = _inputValue * _inputValue;
        }
        if (tempWindowIndex >= inputTotalWindows) tempWindowIndex = 0;
    }

    // computing product of square root sum squared of template and input
    _inputSqrtSumSq = sqrt(_inputSqrtSumSq) * this->templateSqrtSumSq;
    // computing inverse of product (to reduce use of division)
    float _inverseSqrtSumSq = _inputSqrtSumSq > 0.0 ? 1.0 / freqsSqrtSumSq : 1.0;

    // computing dot product and correlation coefficient
    _tempInputWindowIndex = _inputWindowIndex;
    float _correlationCoefficient = 0.0;
    uint32_t _templateValue = 0;
    for (uint16_t t = 0; t < this->numCols; t++) {
        for (uint16_t f = this->frequencyStartIndex; f < this->frequencyEndIndex; f++) {
            _inputValue = *((this->inputPtr + f) + _tempWindowIndex++ * this->numRows);
            _templateValue = *((this->bufferPtr + f) + t * this->numRows);
            _correlationCoefficient += _inputValue * _templateValue * _inverseSqrtSumSq;
        }
        if (tempWindowIndex >= inputTotalWindows) tempWindowIndex = 0;
    }

    return _correlationCoefficient;
}
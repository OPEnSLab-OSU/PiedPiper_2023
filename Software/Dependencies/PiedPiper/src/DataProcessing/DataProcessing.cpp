#include "DataProcessing.h"

void DCRemoval(complex *input, uint16_t windowSize) {
    int i;
    complex average = 0;
    for (i = 0; i < windowSize; i++) {
        average += input[i];
    }

    average /= windowSize;

    for (i = 0; i < windowSize; i++) {
        input[i] -= average;
    }
}

void ComplexToMagnitude(complex *input, uint16_t windowSize) {
    for (int i = 0; i < windowSize; i++) {
        input[i] = sqrt(sq(input[i].re()) + sq(input[i].im()));
    }
}
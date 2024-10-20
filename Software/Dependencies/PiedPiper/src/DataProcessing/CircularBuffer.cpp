#include "DataProcessing.h"

template <class T> CircularBuffer::CircularBuffer() {
    this->bufferPtr = NULL;
    this->bufferIndex = 0;
    this->numRows = 0;
    this->numCols = 0;
}

void CircularBuffer::setBuffer(T *bufferPtr, uint16_t numRows, uint16_t numCols) {
    this->bufferPtr = bufferPtr;

    this->numRows = numRows;
    this->numCols = numCols;
}

void CircularBuffer::pushData(T *data) {
    for (int i = 0; i < this->numRows; i++) {
        *((this->bufferPtr + i) + this->bufferIndex * this->numRows) = data[i];
    }

    this->bufferIndex += 1;
    if (this->bufferIndex == this->numCols) this->bufferIndex = 0;
}

T *getCurrentData(void) {
    return *(this->bufferPtr * this->bufferIndex);
}

T *getData(int relativeIndex) {
    uint16_t _index = (this->bufferIndex + this->numCols + relativeIndex) % this->numCols;
    return this->bufferPtr[_index];
}


T **getBuffer(void) {
    return this->bufferPtr;
}

void clearBuffer(void) {
    for (int t = 0; t < this->numCols; t++) {
        for (int f = 0; f < this->numRows; f++) {
            *((this->bufferPtr + f) + t * this->numRows) = 0;
        }
    }
}
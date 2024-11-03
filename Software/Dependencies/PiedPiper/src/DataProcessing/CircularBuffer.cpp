// #include "DataProcessing.h"

// template <typename T>
// CircularBuffer<T>::CircularBuffer() {
//     this->bufferPtr = NULL;
//     this->bufferIndex = 0;
//     this->numRows = 0;
//     this->numCols = 0;
// }

// template <typename T>
// void CircularBuffer<T>::setBuffer(T *bufferPtr, uint16_t numRows, uint16_t numCols) {
//     this->bufferPtr = bufferPtr;

//     this->numRows = numRows;
//     this->numCols = numCols;
// }

// template <typename T>
// uint16_t CircularBuffer<T>::getNumRows(void) const { return this->numRows; }

// template <typename T>
// uint16_t CircularBuffer<T>::getNumCols(void) const { return this->numCols; }

// template <typename T>
// void CircularBuffer<T>::pushData(T *data) {
//     for (int i = 0; i < this->numRows; i++) {
//         *((this->bufferPtr + i) + this->bufferIndex * this->numRows) = data[i];
//     }

//     this->bufferIndex += 1;
//     if (this->bufferIndex == this->numCols) this->bufferIndex = 0;
// }

// template <typename T>
// T *CircularBuffer<T>::getCurrentData(void) { return *(this->bufferPtr + this->numRows * this->bufferIndex); }

// template <typename T>
// uint16_t CircularBuffer<T>::getCurrentIndex(void) const { return this->bufferIndex; }

// template <typename T>
// T *CircularBuffer<T>::getData(int relativeIndex) {
//     uint16_t _index = (this->bufferIndex + this->numCols + relativeIndex) % this->numCols;
//     return this->bufferPtr + _index * this->numRows;
// }

// template <typename T>
// T *CircularBuffer<T>::getBuffer(void) {
//     return this->bufferPtr;
// }

// template <typename T>
// void CircularBuffer<T>::clearBuffer(void) {
//     for (int t = 0; t < this->numCols; t++) {
//         for (int f = 0; f < this->numRows; f++) {
//             *((this->bufferPtr + f) + t * this->numRows) = 0;
//         }
//     }
//     this->bufferIndex = 0;
// }
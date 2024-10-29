#ifndef DATAPROCESSING_h
#define DATAPROCESSING_h

#include <ArduinoFFT.h>

/*
 * arrayToPointerArray(...) - converts a 2d array to an array of pointers (NOTE: may remove this and just use pointer arithmetic instead)
 *
 * @param *array - pointer to 2DArray (i.e. (float *)2DArray)
 * @param **ptrArray - output array (array of pointers)
 * @param numRows - number of rows in the 2D array
 * @param numCols - number of columns in 2D array
 * 
 * @note The ptrArray must be allocated so that it has the same number of pointers as the number of columns in the 2D array
 */
template <typename T> void arrayToPointerArray(T *array, T **ptrArray, uint16_t numRows, uint16_t numCols);

/*
 * FFT(...) - uses ArduinoFFT to transform a time domain signal to frequency domain
 * 
 * @param *inputReal - a pointer to input array for real samples and output array containing magnitudes
 * @param *inputImag - a pointer to input array for imaginary values (pass an array of zeroes)
 * @param windowSize - sample size of input signal
 */
void FFT(float *inputReal, float *inputImag, uint16_t windowSize);

/*
 * TimeSmoothing(...) - smoothes a spectrogram by averaging through time
 * 
 * @param **input - a pointer to an array of pointers containing the input data
 * @param *output - a pointer to an array for the output data
 * @param windowSize - the window size of the frequency domain
 * @param numWindows - number of windows in spectrogram
 */
template <typename T> void TimeSmoothing(T *input, T *output, uint16_t windowSize, uint16_t smoothingSize);

/*
 * FrequencySmoothing(...) - smoothes a frequency domain by averaging all samples within some bounds
 * 
 * @param *input - a pointer to an array containing the input data
 * @param *output - a pointer to an array for the output data
 * @param windowSize - the window size of the frequency domain
 * @param smoothingSize - number of samples to use around sample for smoothing
 */
template <typename T> void FrequencySmoothing(T *input, T *output, uint16_t windowSize, uint16_t smoothingSize);

/* 
 * AlphaTrimming(...) - removes stochastic noise from a frequency domain
 * 
 * @param *input - a pointer to an array containing the input data
 * @param *output - a pointer to an array for the output data
 * @param windowSize - the window size of the frequency domain
 * @param smoothingSize - number of samples to use around sample for computing standard deviation of sample
 */
template <typename T> void AlphaTrimming(T *input, T *output, uint16_t windowSize, uint16_t smoothingSize, float deviationThreshold);


/*
 *
 *
 * 
 */
template <class T> class CircularBuffer
{
    private:
        T **bufferPtr;

        uint8_t bufferIndex;

        uint16_t numRows;
        uint16_t numColumns;

    public:
        CircularBuffer(void);

        void setBuffer(T *bufferPtr, uint16_t numRows, uint16_t numColumns)

        void pushData(T *data);

        T *getCurrentData(void);

        T *getData(int relativeIndex);

        T **getBuffer(void);

        void clearBuffer(void);

}

/*
 *
 *
 * 
 */
class CrossCorrelation
{
    private:
        uint16_t *templatePtr;

        uint32_t templateSqrtSumSq;

        uint16_t numRows;
        uint16_t numColumns;

        uint16_t sampleRate;
        uint16_t windowSize;
        uint16_t frequencyIndexLow;
        uint16_t frequencyIndexHigh;

        void computeTemplate(void);
    
    public:
        CrossCorrelation(uint16_t sampleRate, uint16_t windowSize);

        void setTemplate(uint16_t *input, uint16_t numRows, uint16_t numCols, uint16_t frequencyRangeLow, uint16_t frequencyRangeHigh);

        float correlate(uint16_t *inputPtr, uint16_t inputStartWindowIndex, uint16_t inputTotalWindows = this->numCols);
};

#endif
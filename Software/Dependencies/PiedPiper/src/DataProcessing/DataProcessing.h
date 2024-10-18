#ifndef DATAPROCESSING_h
#define DATAPROCESSING_h

#include <ArduinoFFTFloat.h>

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
void CalculateDownsampleSincFilter(void);

/*
 *
 *
 * 
 */
void CalculateUpsampleSincFilter(void);

template <class T> class CircularBuffer
{
    private:
        T **bufferPtr;

        uint8_t bufferIndex;

        uint16_t numRows;
        uint16_t numColumns;

    public:
        CircularBuffer(void);

        void setBuffer(T **bufferPtr, uint16_t numRows, uint16_t numColumns)

        void pushData(T *data);

        T *getCurrentData(void);

        T *getData(int relativeIndex);

        T **getBuffer(void);

        void clearBuffer(void);

}

template <class T> class CrossCorrelation
{
    private:
        T **templatePtr;

        float templateSumSquared;

        uint16_t numRows;
        uint16_t numColumns;

        void computeTemplate();
    
    public:
        CrossCorrelation();

        void setTemplate(T **input, uint16_t numRows, uint16_t numColumns);

        float correlate(T **input);
};

#endif
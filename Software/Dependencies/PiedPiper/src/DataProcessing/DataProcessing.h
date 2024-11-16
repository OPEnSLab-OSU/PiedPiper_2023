#ifndef DATAPROCESSING_h
#define DATAPROCESSING_h

#include <arduinoFFT.h>

// ArduinoFFT<float> fft = ArduinoFFT<float>();

/*
 * FFT(...) - uses ArduinoFFT to transform a time domain signal to frequency domain
 * 
 * @param *inputReal - a pointer to input array for real samples and output array containing magnitudes
 * @param *inputImag - a pointer to input array for imaginary values (pass an array of zeroes)
 * @param windowSize - sample size of input signal
 */
void FFT(float *inputReal, float *inputImag, uint16_t windowSize);

void iFFT(float *inputReal, float *inputImag, uint16_t windowSize);

/*
 * TimeSmoothing(...) - smoothes a spectrogram by averaging through time
 * 
 * @param **input - a pointer to an array of pointers containing the input data
 * @param *output - a pointer to an array for the output data
 * @param windowSize - the window size of the frequency domain
 * @param numWindows - number of windows in spectrogram
 */
template <typename T>
void TimeSmoothing(T *input, T *output, uint16_t numRows, uint16_t numCols) {
    T sum = 0;
    uint16_t freq, time;

    float _numWindows = 1.0 / numCols;
    for (freq = 0; freq < numRows; freq++) {
        sum = 0;
        output[freq] = 0;
        for (time = 0; time < numCols; time++) {
            sum += *(input + freq + time * numCols);
        }
        output[freq] = uint16_t(round(sum * _numWindows));
    }   
};

/*
 * FrequencySmoothing(...) - smoothes a frequency domain by averaging all samples within some bounds
 * 
 * @param *input - a pointer to an array containing the input data
 * @param *output - a pointer to an array for the output data
 * @param windowSize - the window size of the frequency domain
 * @param smoothingSize - number of samples to use around sample for smoothing
 */
template <typename T>
void FrequencySmoothing(T *input, T *output, uint16_t numRows, uint16_t smoothingSize) {
    uint16_t freq, samp, startIdx, endIdx;    

    for (freq = 0; freq < numRows; freq++) {
        startIdx = max(0, freq - smoothingSize);
        endIdx = min(numRows - 1, freq + smoothingSize);
        output[freq] = 0;
        for (samp = startIdx; samp <= endIdx; samp++) {
            output[freq] += input[samp];
        }
        output[freq] /= endIdx - startIdx + 1;
    }
};

/* 
 * AlphaTrimming(...) - removes stochastic noise from a frequency domain
 * 
 * @param *input - a pointer to an array containing the input data
 * @param *output - a pointer to an array for the output data
 * @param windowSize - the window size of the frequency domain
 * @param smoothingSize - number of samples to use around sample for computing standard deviation of sample
 */
template <typename T>
void AlphaTrimming(T *input, T *output, uint16_t numRows, uint16_t smoothingSize, float deviationThreshold) {
    uint16_t i, s, startIdx, endIdx, boundNumSamples;
    float boundSum, boundAvg, boundStdDev, temp;

    // copy data to output array to use as scratchpad array
    for (i = 0; i < numRows; i++) {
        output[i] = input[i];
    }

    for (i = 0; i < numRows; i++) {
        // calculate lower and upper bounds based on smoothingSize
        startIdx = max(0, i - smoothingSize);
        endIdx = min((numRows) - 1, i + smoothingSize);
        boundNumSamples = endIdx - startIdx + 1;
        temp = 1.0 / boundNumSamples;

        // get average of magnitudes within lower and upper bound
        boundSum = 0;
        for (s = startIdx; s <= endIdx; s++) {
            boundSum += input[s];
        }
        boundAvg = boundSum * temp;

        // get standard deviation of magnitudes within lower and upper bound
        boundStdDev = 0;
        for (s = startIdx; s <= endIdx; s++) {
            boundStdDev += pow(input[s] - boundAvg, 2);
        }
        boundStdDev = sqrt(boundStdDev * temp);

        // check deviation of each sample within lower and upper bound. If sample deviation is greater than some threshold, 
        // replace sample in subtraction data with the average of the bound excluding this sample
        boundNumSamples -= 1;
        for (s = startIdx; s <= endIdx; s++) {
            if ((input[s] - boundAvg) / boundStdDev > deviationThreshold)
                output[s] = (boundSum - input[s]) / boundNumSamples;
        }
    }

    // subtraction of trimmed data from raw data
    for (i = 0; i < numRows; i++) {
        output[i] = input[i] - output[i];
    }
};


/*
 *
 *
 * 
 */
template <typename T>
class CircularBuffer
{
    private:
        T *bufferPtr;

        uint16_t bufferIndex;

        uint16_t numRows;
        uint16_t numCols;

    public:
        CircularBuffer(void) {
            this->bufferPtr = NULL;
            this->bufferIndex = 0;
            this->numRows = 0;
            this->numCols = 0;
        };

        void setBuffer(T *bufferPtr, uint16_t numRows, uint16_t numCols) {
            this->bufferPtr = bufferPtr;

            this->numRows = numRows;
            this->numCols = numCols;
        };

        uint16_t getNumRows(void) const { return this->numRows; };

        uint16_t getNumCols(void) const { return this->numCols; };

        void pushData(T *data) {
            this->bufferIndex += 1;
            if (this->bufferIndex == this->numCols) this->bufferIndex = 0;

            for (int i = 0; i < this->numRows; i++) {
                *(this->bufferPtr + i + this->numRows * this->bufferIndex) = data[i];
            }
        };
        
        uint16_t getCurrentIndex(void) const { return this->bufferIndex; };

        T *getCurrentData(void) { return this->bufferPtr + this->numRows * this->bufferIndex; };

        T *getData(int relativeIndex) {
            uint16_t _index = (this->bufferIndex + this->numCols + relativeIndex) % this->numCols;
            return this->bufferPtr + _index * this->numRows;
        };

        T *getBuffer(void) { return this->bufferPtr; };

        void clearBuffer(void) {
            for (int t = 0; t < this->numCols; t++) {
                for (int f = 0; f < this->numRows; f++) {
                    *(this->bufferPtr + f + t * this->numRows) = 0;
                }
            }
            this->bufferIndex = 0;
        };

};

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
        uint16_t numCols;

        uint16_t sampleRate;
        uint16_t windowSize;
        float frequencyWidth;
        uint16_t frequencyIndexLow;
        uint16_t frequencyIndexHigh;

        void computeTemplate(void);
    
    public:
        CrossCorrelation(uint16_t sampleRate, uint16_t windowSize);

        void setTemplate(uint16_t *input, uint16_t numRows, uint16_t numCols, uint16_t frequencyRangeLow, uint16_t frequencyRangeHigh);

        float correlate(uint16_t *input, uint16_t inputLatestWindowIndex, uint16_t inputTotalWindows);
};

#endif
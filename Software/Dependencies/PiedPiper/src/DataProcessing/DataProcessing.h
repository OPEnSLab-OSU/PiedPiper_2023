#ifndef DATAPROCESSING_h
#define DATAPROCESSING_h

#include <Fast4ier.h>

/**
 * removes DC noise from a time domain signal by subtracting mean of samples
 * @param input a pointer to complex array
 * @param windowSize length of input array
 */
void DCRemoval(complex *input, uint16_t windowSize);


/**
 * converts an array of complex values to magnitudes
 *
 * @param input a pointer to a complex array
 * @param windowSize length of input array
 */
void ComplexToMagnitude(complex *input, uint16_t windowSize);


/**
 * smoothes a spectrogram by averaging through time
 * @param input a pointer to a 2d array containing the input data
 * @param output a pointer to an array for the output data
 * @param numRows number of amplitudes per window
 * @param numCols number of windows in spectrogram
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


/**
 * smoothes a frequency domain by averaging all samples within some bounds
 * @param input a pointer to an array containing the input data
 * @param output a pointer to an array for the output data
 * @param numRows number of elements in input array
 * @param smoothingSize number of samples to use around sample for smoothing
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


/**
 * noise removal using CFAR (Constant False Alarm Rate) algorithm
 * @param input a pointer to an array containing the input data
 * @param output a pointer to an array for the output data
 * @param numRows number of elements in input array
 * @param numGapCells number of gap cells to use for computing noise gate
 * @param numRefCells number of reference cells to use for computing noise gate
 * @param bias multiplier for noise gate
 */
template <typename T>
void NoiseRemoval_CFAR(T* input, T* output, uint16_t numRows, uint16_t numGapCells, uint16_t numRefCells, float bias) {
    uint16_t i, j, startIdx, gapStartIdx, gapEndIdx, endIdx, numCells;
    float cellSum;

    for (i = 0; i < numRows; i++) {

        cellSum = 0;
        numCells = 0;

        // calculating bounds for this sample
        startIdx = max(0, i - numRefCells - numGapCells);
        endIdx = min(numRows - 1, i + numRefCells + numGapCells);
        gapStartIdx = max(0, i - numGapCells);
        gapEndIdx = min(numRows - 1, i + numGapCells);

        // computing sum of cells within bounds
        for (j = startIdx; j < gapStartIdx; j++) {
            numCells += 1;
            cellSum += input[j];
        }
        for (j = gapStartIdx + 1; j <= endIdx; j++) {
            numCells += 1;
            cellSum += input[j];
        }

        // computing average of cells within bounds
        cellSum /= numCells > 0 ? numCells : 1;

        // checking if sample exceeds threshold, if not sample is zeroed out
        if (input[i] > cellSum * bias) output[i] = input[i];
        else output[i] = 0;
    }
};


/** 
 * noise removal using ATM (Alpha-Trimmed Mean) algorithm
 * @param input a pointer to an array containing the input data
 * @param output a pointer to an array for the output data
 * @param numRows number of elements in input array
 * @param smoothingSize number of samples to use around sample for computing standard deviation of sample
 */
template <typename T>
void NoiseRemoval_ATM(T *input, T *output, uint16_t numRows, uint16_t smoothingSize, float deviationThreshold) {
    uint16_t i, s, startIdx, endIdx, boundNumSamples;
    float boundSum, boundAvg, boundStdDev, temp;

    // copy data to output array to use as scratchpad array
    for (i = 0; i < numRows; i++) {
        output[i] = input[i];
    }

    for (i = 0; i < numRows; i++) {
        // calculate lower and upper bounds based on smoothingSize
        startIdx = max(0, i - smoothingSize);
        endIdx = min(numRows - 1, i + smoothingSize);
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
 * templated class for circular buffer
 */
template <typename T>
class CircularBuffer
{
    private:
        T *bufferPtr;           ///< pointer to some buffer   

        uint16_t bufferIndex;   ///< current index in buffer

        uint16_t numRows;       ///< number of rows in buffer
        uint16_t numCols;       ///< number of columns in buffer

    public:

        /**
         * constructor for CircularBuffer
         */
        CircularBuffer(void) {
            this->bufferPtr = NULL;
            this->bufferIndex = 0;
            this->numRows = 0;
            this->numCols = 0;
        };

        /**
         * set some buffer/array to be used as a circular buffer
         * @param bufferPtr pointer to buffer
         * @param numRows number of rows in buffer
         * @param numCols number of columns in buffer
         */
        void setBuffer(T *bufferPtr, uint16_t numRows, uint16_t numCols) {
            this->bufferPtr = bufferPtr;

            this->numRows = numRows;
            this->numCols = numCols;
        };

        /**
         * get number of rows in buffer
         * @return number of rows
         */
        uint16_t getNumRows(void) const { return this->numRows; };

        /**
         * get number of columns in buffer
         * @return number of columns
         */
        uint16_t getNumCols(void) const { return this->numCols; };

        /**
         * push some data to circular buffer
         * @param data data to push onto buffer
         */
        void pushData(T *data) {
            this->bufferIndex += 1;
            if (this->bufferIndex == this->numCols) this->bufferIndex = 0;

            for (int i = 0; i < this->numRows; i++) {
                *(this->bufferPtr + i + this->numRows * this->bufferIndex) = data[i];
            }
        };
        
        /**
         * get the current column in circular buffer
         * @return current column in circular buffer
         */
        uint16_t getCurrentIndex(void) const { return this->bufferIndex; };

        /**
         * get data in current column of circular buffer
         * @return data in current column of circular buffer
         */
        T *getCurrentData(void) { return this->bufferPtr + this->numRows * this->bufferIndex; };

        /**
         * get data at some index relative to the current index of circular buffer
         * @param relativeIndex a positive or negative value
         * @return column of data at relative index
         */
        T *getData(int relativeIndex) {
            uint16_t _index = (this->bufferIndex + this->numCols + relativeIndex) % this->numCols;
            return this->bufferPtr + _index * this->numRows;
        };
        
        /**
         * get the entire buffer as a pointer
         * @return pointer to buffer
         */
        T *getBuffer(void) { return this->bufferPtr; };

        /**
         * zeroes out entire buffer
         */
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
 * class for computing correlation coefficient between two signals
 */
class CrossCorrelation
{
    private:
        uint16_t *templatePtr;          ///< pointer to buffer containing template data

        uint32_t templateSqrtSumSq;     ///< stores the square root of the sum squared of template

        uint16_t numRows;               ///< number of rows in template
        uint16_t numCols;               ///< number of columns in template

        uint16_t sampleRate;            ///< sample rate of template
        uint16_t windowSize;            ///< window size of template
        float frequencyWidth;           ///< value to use for template values scaling
        uint16_t frequencyIndexLow;     ///< bin index corresponding to lowest frequency
        uint16_t frequencyIndexHigh;    ///< bin index corresponding to highest frequency 

        /**
         * preliminary computations of template data for correlation
         */
        void computeTemplate(void);
    
    public:
        /**
         * constructor for CrossCorrelation
         * @param sampleRate sample rate of signal
         * @param windowSize window size of signal
         */
        CrossCorrelation(uint16_t sampleRate, uint16_t windowSize);

        /**
         * set some buffer as template for cross correlation
         * @param input a pointer to some buffer containing template data
         * @param numRows number of rows in data
         * @param numCols number of columns in data
         * @param frequencyRangeLow start frequency for correlation
         * @param frequencyRangeHigh end frequency for correlation
         */
        void setTemplate(uint16_t *input, uint16_t numRows, uint16_t numCols, uint16_t frequencyRangeLow, uint16_t frequencyRangeHigh);

        /**
         * computes correlation coefficient between template and input signal
         * @param input a pointer to buffer containing data
         * @param inputLatestWindowIndex column at which to start correlation
         * @param inputTotalWindows total columns in input signal
         */
        float correlate(uint16_t *input, uint16_t inputLatestWindowIndex, uint16_t inputTotalWindows);
};

#endif
#ifndef DATAPROCESSING_h
#define DATAPROCESSING_h

#include <ArduinoFFTFloat.h>

void FFT(float *inputReal, float *inputImag, uint16_t windowSize);
template <typename T> void TimeSmoothing(T *input, T *output, uint16_t windowSize, uint16_t smoothingSize);
template <typename T> void FrequencySmoothing(T *input, T *output, uint16_t windowSize, uint16_t smoothingSize);
template <typename T> void AlphaTrimming(T *input, T *output, uint16_t windowSize, uint16_t smoothingSize, float deviationThreshold);

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
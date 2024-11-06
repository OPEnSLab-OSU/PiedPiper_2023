#ifndef PiedPiper_h
#define PiedPiper_h

#include <Arduino.h>
#include <SD.h>
#include <Wire.h>
// #include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include "PiedPiperSettings.h"
#include "Devices/Peripherals.h"
#include "Other/OperationManager.h"
#include "DataProcessing/DataProcessing.h"

const uint16_t ADC_MAX = (1 << ADC_RESOLUTION) - 1;
const uint16_t DAC_MAX = (1 << DAC_RESOLUTION) - 1;

const uint16_t ADC_MID = 1 << (ADC_RESOLUTION - 1);
const uint16_t DAC_MID = 1 << (DAC_RESOLUTION - 1);

const uint16_t FFT_WINDOW_SIZE = WINDOW_SIZE / AUD_IN_DOWNSAMPLE_RATIO;
const uint16_t FFT_WINDOW_SIZE_BY2 = FFT_WINDOW_SIZE >> 1;
const uint16_t FFT_SAMPLE_RATE = SAMPLE_RATE / AUD_IN_DOWNSAMPLE_RATIO;
const uint16_t AUD_OUT_SAMPLE_RATE = SAMPLE_RATE * AUD_OUT_UPSAMPLE_RATIO;

const float FREQ_WIDTH = float(FFT_WINDOW_SIZE) / FFT_SAMPLE_RATE;

const uint16_t AUD_IN_SAMPLE_DELAY_TIME = 1000000 / SAMPLE_RATE;
const uint16_t AUD_OUT_SAMPLE_DELAY_TIME = 1000000 / AUD_OUT_SAMPLE_RATE;

/*
    Pied Piper Base is the base class of Pied Piper Monitor and Playback. This class contains functions and data structures which are 
    relevant to all child classes, such as loading and writing data to and SD card, initializing timer interrupt for sampling
    or outputting a signals, etc. Note: hardware configuration used for child classes may differ!
*/
class PiedPiperBase
{

    protected:
        static uint16_t PLAYBACK_FILE[SAMPLE_RATE * PLAYBACK_FILE_LENGTH];

        static uint16_t PLAYBACK_FILE_SAMPLE_COUNT;

        static volatile uint16_t PLAYBACK_FILE_BUFFER_IDX;

        static void RESET_PLAYBACK_FILE_INDEX(void);

    private:

        void configurePins(void);
        
        static void calculateDownsampleSincFilterTable(void);
        static void calculateUpsampleSincFilterTable(void);

        static void RecordSample(void);
        static void OutputSample(void);
        static void RecordAndOutputSample(void);

    public:
    
        PiedPiperBase();        

        Adafruit_NeoPixel indicator = Adafruit_NeoPixel(1, 8, NEO_GRB + NEO_KHZ800);

        static TimerInterruptController TimerInterrupt;
        SleepController SleepControl;
        WDTController WDTControl;
        OperationManager OperationMan;

        SDWrapper SDCard = SDWrapper(PIN_SD_CS);

        RTCWrapper RTCWrap = RTCWrapper(DEFAULT_DS3231_ADDR);

        char calibrationFilename[32];
        char playbackFilename[32];
        char templateFilename[32];
        char operationTimesFilename[32];

        virtual void init(void);

        void Hypnos_3VR_ON(void);
        void Hypnos_3VR_OFF(void);

        void Hypnos_5VR_ON(void);
        void Hypnos_5VR_OFF(void);

        static void startAudioInput(void);
        static void startAudioInputAndOutput(void);
        static void stopAudio(void);
        static void performPlayback(void);

        void initializationFail(void);
        void initializationSuccess(void);

        bool loadSettings(char *filename);

        bool loadSound(char *filename);

        bool loadOperationTimes(char *filename);

        bool loadTemplate(char *filename, uint16_t *bufferPtr, uint16_t templateLength);

        template <typename T> void writeArrayToFile(T *array, uint16_t arrayLength) {
            for (int i = 0; i < arrayLength; i++) {
                SDCard.data.println(array[i], DEC);
            }
        };

        template <typename T> void writeCircularBufferToFile(CircularBuffer<T> *buffer) {
            uint16_t _rows = buffer->getNumRows();
            uint16_t _cols = buffer->getNumCols();
            T *_column = 0;
            for (int i = 1; i <= _cols; i++) {
                _column = buffer->getData(i);
                for (int j = 0; j < _rows; j++) {
                    SDCard.data.println(_column[j], DEC);
                }
            }
        };

        static bool audioInputBufferFull(uint16_t *bufferPtr);
};

/* 
    Pied Piper Monitor primary purpose is to sample a signal from tactile microphone and run a template based detection algorithm on the sampled 
    data along with luring pests by playing mating call. It can also:
     - Operate during certain times of day (using "STANDBY" sleep) to conserve battery life
     - Write data to SD such as: photos from TTL camera, raw samples and spectrogram of signal recorded by microphone, temperature and humidity,
       and date/time
     - Perform playback intermittently and/or in response to a detection
*/
class PiedPiperMonitor : public PiedPiperBase
{
    private:

    public:

        DFRobot_SHT3x tempSensor = DFRobot_SHT3x();

        MCP465 preAmp = MCP465(DEFAULT_MCP465_ADDR);

        TTLCamera camera = TTLCamera();

        PAM8302 amp = PAM8302(PIN_AMP_SD);

        PiedPiperMonitor();

        void init();

        void frequencyResponse();

        void calibrate(uint16_t calibrationValue, uint16_t threshold);



};

/*
    Pied Piper Playback is used purely for continuously performing playback throughout operation intervals (times of day when the trap should be 
    active (using "OFF" sleep).
*/
class PiedPiperPlayback : public PiedPiperBase
{
    private:

    public:

        PAM8302 amp = PAM8302(PIN_AMP_SD);

        PiedPiperPlayback();

        void init();

};

#endif
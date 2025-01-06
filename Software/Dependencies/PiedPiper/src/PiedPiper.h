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

const uint16_t ADC_MAX = (1 << ADC_RESOLUTION) - 1; ///< Maximum write value of ADC
const uint16_t DAC_MAX = (1 << DAC_RESOLUTION) - 1; ///< Maximum write value of DAC

const uint16_t ADC_MID = 1 << (ADC_RESOLUTION - 1); ///< Mid value of ADC
const uint16_t DAC_MID = 1 << (DAC_RESOLUTION - 1); ///< Mid valie of DAC

const uint16_t FFT_WINDOW_SIZE = WINDOW_SIZE / AUD_IN_DOWNSAMPLE_RATIO;     ///< number of samples per window with resampling
const uint16_t FFT_WINDOW_SIZE_BY2 = FFT_WINDOW_SIZE >> 1;                  ///< FFT_WIDOW_SIZE divided by 2
const uint16_t FFT_SAMPLE_RATE = SAMPLE_RATE / AUD_IN_DOWNSAMPLE_RATIO;     ///< sample rate of resampled input signal
const uint16_t AUD_OUT_SAMPLE_RATE = SAMPLE_RATE * AUD_OUT_UPSAMPLE_RATIO;  ///< sample rate of resampled output signal

const float FREQ_WIDTH = float(FFT_WINDOW_SIZE) / FFT_SAMPLE_RATE;          ///< A constant to help normalize FFT data...probably better to simply dividd the FD by number of samples

const uint16_t AUD_IN_SAMPLE_DELAY_TIME = 1000000 / SAMPLE_RATE;            ///< Delay between audio input samples
const uint16_t AUD_OUT_SAMPLE_DELAY_TIME = 1000000 / AUD_OUT_SAMPLE_RATE;   ///< Delay between audio output samples

/**
 * States which the sampling ISR can be in
 */
enum AUD_STATE {
    AUD_STOP = 0,   ///< Audio stopped, ISR detached
    AUD_IN,         ///< Audio input only, ISR runs RecordSample()
    AUD_OUT,        ///< Audio output only, ISR runs OutputSample()
    AUD_IN_OUT      ///< Audio input and output, ISR runs RecordAndOutputSample()
};

/**
 * Pied Piper Base is the base class of Pied Piper Monitor and Playback. This class contains functions and data structures which are 
 * relevant to all child classes, such as loading and writing data to and SD card, initializing timer interrupt for sampling
 * or outputting a signals, etc.
 * @note hardware configuration used for child classes may differ!
*/
class PiedPiperBase
{

    protected:
        static uint16_t PLAYBACK_FILE[SAMPLE_RATE * PLAYBACK_FILE_LENGTH];  ///< stores samples for audio playback, these can be manually inserted via member functions or loaded from SD card with loadSound(...)

        static uint16_t PLAYBACK_FILE_SAMPLE_COUNT;                         ///< stores the number of samples in PLAYBACK_FILE, meaning that the PLAYBACK_FILE can contain any number of samples as long as it is less than or equal to SAMPLE_RATE * PLAYBACK_FILE_LENGTH

        static void RESET_PLAYBACK_FILE_INDEX(void);                        ///< sets the playback file index to 0, meaing the next sample to be played is the first sample in PLAYBACK_FILE

    private:

        static AUD_STATE audState;  ///< stores state of sampling timer ISR

        /**
         * sets pinMode() for all pins in use
         */
        void configurePins(void);
        
        /**
         * calculates sinc filter table for resampling input signal
         */
        static void calculateDownsampleSincFilterTable(void);
        /**
         * calculates sinc filter table for resampling output signal
         */
        static void calculateUpsampleSincFilterTable(void);

        /**
         * sets delta spike as flattening filter in case impulse response is not or cannot be calculated
         */
        static void generateImpulse(void);

        /**
         * records/resamples a single sample and stores to AUD_IN_BUFFER
         */
        static void RecordSample(void);
        /**
         * outputs/resamples a single sample from PLAYBACK_BUFFER
         */
        static void OutputSample(void);
        /**
         * runs RecordSample() and OutputSample() together
         */
        static void RecordAndOutputSample(void);
        /**
         * records/outputs a single raw sample (no resampling is done)
         */
        static void RecordAndOutputRawSample(void);

    public:
    
        /**
         * constructor for PiedPiperBase
         */
        PiedPiperBase();

        Adafruit_NeoPixel indicator = Adafruit_NeoPixel(1, 8, NEO_GRB + NEO_KHZ800);    ///< LED indicator on M4 Express

        static TimerInterruptController TimerInterrupt;         ///< Object for controlloring timer interrupt
        SleepController SleepControl;                           ///< Object for putting MCU to sleep
        WDTController WDTControl;                               ///< WatchDog timer for resetting board in case there is an issue 
        OperationManager OperationMan;                          ///< Object for setting operation and checking which state device should be in

        SDWrapper SDCard = SDWrapper(PIN_SD_CS);                ///< Wrapper for SD

        RTCWrapper RTCWrap = RTCWrapper(DEFAULT_DS3231_ADDR);   ///< Wrapper for DS3213 RTC

        char calibrationFilename[32];       ///< stores directory of calibration file
        char playbackFilename[32];          ///< stroes directory of playback file
        char templateFilename[32];          ///< stores directory of correlation template file
        char operationTimesFilename[32];    ///< stores directory of operation times file

        /**
         * sets pinMode() on all pins in use, runs preliminary calculations such as sinc filter tables, and initializes timer
         */
        virtual void init(void);

        void HYPNOS_3VR_ON(void);
        void HYPNOS_3VR_OFF(void);

        void HYPNOS_5VR_ON(void);
        void HYPNOS_5VR_OFF(void);

        /**
         * attaches RecordSample() to timer ISR
         */
        static void startAudioInput(void);
        /**
         * attaches RecordAndOutputSample() to timer ISR
         */
        static void startAudioInputAndOutput(void);
        /**
         * attaches RecordAndOutputRawSample to timer ISR
         */
        static void startRawAudioInputAndOutput(void);
        /**
         * detaches ISR from timer
         */
        static void stopAudio(void);

        /**
         * performs a single playback of ALL samples stored in PLAYBACK_FILE
         */
        static void performPlayback(void);

        /**
         * calculates a filter to flatten frequency response of audio output by sampling a series of impulses produced by vibration exciter through substrate
         * @param responseAveraging number of impulses used for calculating frequency response
         */
        void impulseResponseCalibration(uint8_t responseAveraging = 1);

        /**
         * Continously flashes NeoPixel LED red and resets the MCU using WDT
         */
        void initializationFail(void);
        /**
         * Flashes NeoPixel LED green once
         */
        void initializationSuccess(void);

        /**
         * loads a settings file from SD card
         * @param filename char array containing directory of settings file (i.e. "SETTINGS.txt")
         * @return False on failure
         * @note see SD template for an example of settings file structure
         */
        bool loadSettings(char *filename);
        /**
         * loads playback sound from SD card
         * @param filename char array containing directory of sound file (i.e. "PBAUD/BMSB.PAD")
         * @return False on failure
         * @note see Documentation for instructions on formatting a playback sound
         */
        bool loadSound(char *filename);
        /**
         * loads operation times from SD card
         * @param filename char array containing directory of operation times file (i.e. "PBINT/PBINT.txt")
         * @return False on failure
         * @note see SD template for an example of operation times file structure
         */
        bool loadOperationTimes(char *filename);
        /**
         * loads correlation template from SD card
         * @param filename char array containing directory of template file (i.e. "TEMPS/BMSB.txt")
         * @param buffer pointer to 2d uint16_t array for storing template data
         * @param templateLength length of template in windows
         * @return False on failure
         * @note see Documentation on instructions for creating a template
         */
        bool loadTemplate(char *filename, uint16_t *bufferPtr, uint16_t templateLength);

        /**
         * this is a templated function which writes some data to an open file
         * @param array pointer to an array
         * @param arrayLength size of array
         * @note this function assumes that the file is already open using the SDCard object in other words it is up to the user to ensure that the file was opened successfully
         */
        template <typename T> void writeArrayToFile(T *array, uint16_t arrayLength) {
            for (int i = 0; i < arrayLength; i++) {
                SDCard.data.println(array[i], DEC);
            }
        };

        /**
         * this is a templated function which writes data stored in a circular buffer object to an open file
         * @param buffer pointer to a CircularBuffer
         */
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

        /**
         * checks if volatile input buffer was filled with samples sampled by ISR.
         * @param bufferOtr uint16_t array with length greater than or equal to FFT_WINDOW_SIZE
         * @return true if buffer was filled, upon which samples are stored to bufferPtr and sampling is resumed
         */
        static bool audioInputBufferFull(uint16_t *bufferPtr);

        /**
         * get index of current sample in playback file
         * @return index of the current sample in playback file
         */
        static uint16_t getPlaybackFileIndex();

        /**
         * checks if full duration of playback file has been played, if so the playback file index is reset
         */
        static void checkResetPlaybackFileIndex();

        /**
         * get the current state of sampling ISR
         * @return AUD_STATE enum
         * @see AUD_STATE
         */
        static AUD_STATE getAudState();
};

/** 
 * Pied Piper Monitor primary purpose is to sample a signal from tactile microphone and run a template based detection algorithm on the sampled 
 * data along with luring pests by playing mating call. It can also:
 *  - Operate during certain times of day (using "STANDBY" sleep) to conserve battery life
 *  - Write data to SD such as: photos from TTL camera, raw samples and spectrogram of signal recorded by microphone, temperature and humidity,
 *    and date/time
 *  - Perform playback intermittently and/or in response to a detection
*/
class PiedPiperMonitor : public PiedPiperBase
{
    private:

    public:

        DFRobot_SHT3x *tempSensor = NULL;               ///< pointer to DFRobot_SHT3x object defined in PiedPiperMonitor.cpp file

        MCP465 preAmp = MCP465(DEFAULT_MCP465_ADDR);    ///< object for digital potentiometer

        TTLCamera camera = TTLCamera();                 ///< object for TTL camera

        PAM8302 amp = PAM8302(PIN_AMP_SD);              ///< object for amplifier

        /**
         * constructor for PiedPiperMonitor
         */
        PiedPiperMonitor();

        /**
         * calls init() from PiedPiperBase 
         */
        void init();

        /**
         * sets gain on digital potentiometer based on amplitude of ADC readings when the PLAYBACK_FILE signal is played through subtrate by vibration exciter
         * 
         * @param adcRange target ADC range [0, 1.0]
         * @param rangeThreshold target ADC range threshold [0, 1.0]
         * @return true if calibration was successful
         */
        bool preAmpGainCalibration(float adcRange, float rangeThreshold);


};

/**
 * Pied Piper Playback is used purely for continuously performing playback throughout operation intervals (times of day when the trap should be 
 * active (using "OFF" sleep).
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
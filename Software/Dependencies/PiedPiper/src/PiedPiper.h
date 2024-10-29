#ifndef PiedPiper_h
#define PiedPiper_h

#include <Arduino.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include "PiedPiperSettings.h"
#include "Peripherals.h"
#include "OperationManager.h"

const uint16_t ADC_MAX = (1 << ADC_RESOLUTION) - 1;
const uint16_t DAC_MAX = (1 << DAC_RESOLUTION) - 1;

const uint16_t ADC_MID = 1 << (ADC_RESOLUTION - 1);
const uint16_t DAC_MID = 1 << (DAC_RESOLUTION - 1);

const uint16_t FFT_WINDOW_SIZE = WINDOW_SIZE / AUD_IN_DOWNSAMPLE_RATIO;
const uint16_t FFT_WINDOW_SIZE_BY2 = FFT_WINDOW_SIZE >> 1;
const uint16_t FFT_SAMPLE_RATE = SAMPLE_RATE / AUD_IN_DOWNSAMPLE_RATIO;
const uint16_t AUD_OUT_SAMPLE_RATE = SAMPLE_RATE * AUD_OUT_UPSAMPLE_RATIO;

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

        static char playbackFilename[32];
        static char templateFilename[32];
        static char operationTimesFilename[32];

    private:

        static uint16_t PLAYBACK_FILE[SAMPLE_RATE * PLAYBACK_FILE_LENGTH];

        static uint16_t PLAYBACK_FILE_SAMPLE_COUNT;

        static void configurePins(void);
        
        static void calculateDownsampleSincFilterTable(void);
        static void calculateUpsampleSincFilterTable(void);

        static void RecordSample(void);
        static void OutputSample(void);

    public:
    
        PiedPiperBase() = 0;

        static Adafruit_NeoPixel indicator(1, 8, NEO_GRB + NEO_KHZ800);

        static TimerInterruptController TimerInterrupt;
        static SleepController SleepController;
        static WDTController WDT;
        static OperationManager operationManager;

        SDWrapper SDCard = SDWrapper();

        RTCWrapper RTC = RTCWrapper(DEFAULT_DS3231_ADDR);

        inline static void Hypnos_3VR_ON(void);
        inline static void Hypnos_3VR_OFF(void);

        inline static void Hypnos_5VR_ON(void);
        inline static void Hypnos_5VR_OFF(void);

        static void startAudioInput(void);
        static void stopAudio(void);
        static void performPlayback(void);

        static void initializationFail(void);
        static void initializationSuccess(void);

        static bool loadSettings(char *filename);

        static bool loadSound(char *filename);

        static bool loadTemplate(char *filename, uint16_t *bufferPtr, uint16_t templateLength);

        static bool loadOperationTimes(char *filename);

        static bool audioInputBufferFull(float *bufferPtr);
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

        DFRobot_SHT3x tempSensor = DFRobot_SHT3x();

        MCP465 preAmp = MCP465(DEFAULT_MCP465_ADDR);

        TTLCamera camera = TTLCamera();

        PAM8302 amp = PAM8302(PIN_AMP_SD);

    public:
        PiedPiperMonitor();

};

/*
    Pied Piper Playback is used purely for continuously performing playback throughout operation intervals (times of day when the trap should be 
    active (using "OFF" sleep).
*/
class PiedPiperPlayback : public PiedPiperBase
{
    private:

        PAM8302 amp = PAM8302(PIN_AMP_SD);

    public:
        PiedPiperPlayback();

};

#endif
#ifndef PiedPiper_h
#define PiedPiper_h

#include <Arduino.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include "PiedPiperSettings.h"
#include "Peripherals.h"

/*
    Pied Piper Base is the base class of Pied Piper Monitor and Playback. This class contains functions and data structures which are 
    relevant to all child classes, such as loading and writing data to and SD card, initializing timer interrupt for sampling
    or outputting a signals, etc. Note: hardware configuration used for child classes may differ!
*/
class PiedPiperBase
{
    protected:

        inline static void Hypnos_3VR_ON(void);
        inline static void Hypnos_3VR_OFF(void);

        inline static void Hypnos_5VR_ON(void);
        inline static void Hypnos_3VR_OFF(void);

        static Adafruit_NeoPixel indicator(1, 8, NEO_GRB + NEO_KHZ800);

        static TimerInterruptController TimerInterrupt;
        static SleepController SleepController;
        static WDTController WDT;

        static volatile uint16_t AUD_IN_BUFFER[WINDOW_SIZE];
        static uint16_t PLAYBACK_FILE[SAMPLE_RATE * PLAYBACK_FILE_LENGTH];

        static void startAudioInput(void);
        static void stopAudio(void);
        static void performPlayback(void);

        static void initializationFail(void);
        static void initializationSuccess(void);

        static bool loadSound(uint16_t *output);

        static bool loadSettings();

        static bool loadTemplate();

        static bool loadOperationTimes();

    private:

        static void RecordSample(void);
        static void OutputSample(void);

    public:
    
        PiedPiperBase();

        virtual void init(void) = 0;

        virtual void update(void) = 0;
};

/* 
    Pied Piper Monitor primary purpose is to sample a signal from tactile microphone and run a template based detection algorithm on the sampled 
    data along with luring pests by playing mating call. It can also:
     - Operate during certain times of day (using "STANDBY" sleep) to conserve battery life
     - Write data to SD such as: photos from TTL camera, raw samples and spectrogram of signal recorded by microphone, temperature and humidity,
       and date/time
     - Perform playback intermittently and/or in response to a detection
*/
class PiedPiperMonitor : PiedPiperBase
{
    private:
        SD sd();

        RTC rtc();

        SHT31 tempSensor();

        MCP465 preAmplifier();

        TTLCamera camera();

        PAM8302 amplifier();

    public:
        PiedPiperMonitor();

        void init(void);

        void update(void);

};

/*
    Pied Piper Playback is used purely for continuously performing playback throughout operation intervals (times of day when the trap should be 
    active (using "OFF" sleep).
*/
class PiedPiperPlayback : PiedPiperBase
{
    private:
        SD sd();

        RTC rtc();

        PAM8302 amplifier();

    public:
        PiedPiperPlayback();
        
        void init(void);

        void update(void);

};

#endif
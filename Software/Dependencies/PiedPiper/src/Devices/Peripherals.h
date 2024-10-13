#ifndef PERIPHERALS_h
#define PERIPHERALS_h

#include <SD.h>
#include <Wire.h>
#include "RTClib.h"
#include "SAMDTimerInterrupt.h"

/** sleep modes available on the Feather M4 Express. */
enum SLEEPMODES
{
    IDLE0 = 0x0,      ///< IDLE0
    IDLE1 = 0x1,      ///< IDLE1
    IDLE2 = 0x2,      ///< IDLE2
    STANDBY = 0x4,    ///< STANDBY sleep mode can exited with a interrupt (this sleep mode is good, but draws ~20mA by default)
    HIBERNATE = 0x5,  ///< HIBERNATE sleep mode can only be exited with a device reset
    BACKUP = 0x6,     ///< BACKUP sleep mode can only be exited with a device reset
    OFF = 0x7         ///< OFF sleep mode can only be exited with a device reset (best sleep mode to use for least power consumption)
};

class TimerInterruptController
{
    private:
        static SAMDTimer ITimer(TIMER_TC3);
        
        static void blankFunction(void);
        
    public:
        static void initialize(void);
        static void attachTimerInterrupt(const unsigned long interval_us, void(*fnPtr)());
        static void detachTimerInterrupt(void);
};

class WDTController
{
    private:

    public:
        WDTController();
        inline void start();
        inline void reset();
        
}

class SleepController
{
    private:

    public:
        SleepController();
        inline void goToSleep(SLEEPMODES sleepMode);
};

class SD
{
    private:

        uint8_t PIN_CS;
        File data;
        
    public:
        SD();

        bool initialize(uint8_t PIN_CS);

        bool begin(void);

        void end(void);

        bool openFile();

        void closeFile(void);

        bool loadSound(uint16_t *output);

        bool loadSettings();

        bool loadOperationTimes();

};

class TTLCamera
{
    private:

    public:
        TTLCamera();
        
        bool initialize(void);

        bool cameraOn();
        bool cameraOff();
        bool takePhoto(char *filename);

};


class RTC
{
    private:
        uint8_t i2c_address;
        RTC_DS3231 rtc;

    public:
        RTC(uint8_t i2c_address);

        initialize(void);

        DateTime readDateTime();

        bool clearAlarm();

        bool setAlarm();

        bool setAlarmSeconds();

};

class MCP465
{
    private:
        uint8_t i2c_address;

    public:
        MCP465(uint8_t i2c_address);

        bool initialize(void);

        bool incrementWiper(void);
        bool decrementWiper(void);

        bool writeWiperValue(uint16_t wiperValue);
        uint16_t readWiperValue(void);

};

class SHT31
{
    private:
        uint8_t i2c_address;

    public:
        SHT31(uint8_t i2c_address);

        bool initialize(void);

        float readTemperature();
        float readHumidity();

};

class PAM8302
{
    private:
        uint8_t PIN_SD;
    
    public:
        PAM8302(uint8_t PIN_SD);

        inline void powerOn();
        inline void powerOff();
}

#endif
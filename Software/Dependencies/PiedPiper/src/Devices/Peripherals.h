#ifndef PERIPHERALS_h
#define PERIPHERALS_h

#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include "RTClib.h"
#include <Adafruit_VC0706.h>
#include <DFRobot_SHT3x.h>

class TimerInterruptController
{
    private:

        static void blankFunction(void);
        
    public:

        TimerInterruptController();

        static void initialize(void);
        static void attachTimerInterrupt(const unsigned long interval_us, void(*fnPtr)());
        static void detachTimerInterrupt(void);
};

class WDTController
{
    private:

    public:

        WDTController();
        void start();
        void reset();
        
};

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


class SleepController
{
    private:

    public:

        SleepController();
        void goToSleep(SLEEPMODES sleepMode);

};

class SDWrapper
{
    private:

        uint8_t PIN_CS;
        
    public:

        File data;

        SDWrapper(uint8_t PIN_CS);

        bool initialize(void);

        bool begin(void);

        void end(void);

        bool openFile(char *filename, uint8_t mode);

        void closeFile(void);

};

class TTLCamera
{
    private:

        Adafruit_VC0706 cam = Adafruit_VC0706(&Serial1);

    public:

        TTLCamera();
        
        bool initialize(void);

        bool cameraOn();
        bool cameraOff();
        bool takePhoto(File *file);

};


class RTCWrapper
{
    private:

        uint8_t i2c_address;

    public:

        RTCWrapper(uint8_t i2c_address);

        RTC_DS3231 rtc;

        bool initialize(void);

        DateTime getDateTime(void);

        void clearAlarm(void);

        bool setAlarm(DateTime alarmDateTime);

        
        bool setAlarmSeconds(int32_t seconds);

};

class MCP465
{
    private:

        uint8_t i2c_address;

    public:

        MCP465(uint8_t i2c_address);

        bool initialize(void);

        uint8_t incrementWiper(void);
        uint8_t decrementWiper(void);

        uint8_t writeWiperValue(uint16_t wiperValue);
        int16_t readWiperValue(void);

};

// class SHT31
// {
//     private:

//         uint8_t i2c_address;

//     public:

//         SHT31(uint8_t i2c_address);

//         bool initialize(void);

//         float readTemperature();
//         float readHumidity();

// };

class PAM8302
{
    private:

        uint8_t PIN_SD;
    
    public:

        PAM8302(uint8_t PIN_SD);

        void powerOn();
        void powerOff();
};

#endif
#ifndef PERIPHERALS_h
#define PERIPHERALS_h

#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include "RTClib.h"
#include <Adafruit_VC0706.h>
#include <DFRobot_SHT3x.h>

/**
 * object for controlling hardware timer interrupt
 */
class TimerInterruptController
{
    private:

        /**
         * empty function for initializing timer
         */
        static void blankFunction(void);
        
    public:

        /**
         * constructor for TimerInterruptController
         */
        TimerInterruptController();

        /**
         * initialize timer interrupt
         */
        static void initialize(void);
        /**
         * attach callback function for timer to call at specified interval
         * @param interval_us microseconds between each alarm
         * @param fnPtr function pointer
         */
        static void attachTimerInterrupt(const unsigned long interval_us, void(*fnPtr)());
        /**
         * detach callback function from timer alarm
         */
        static void detachTimerInterrupt(void);
};

/**
 * object for controlling WatchDog Timer
 */
class WDTController
{
    private:

    public:

        /**
         * consturctor for WDTController
         */
        WDTController();

        /**
         * start WDT, device will be reset in 4 seconds
         */
        void start();
        /** 
         * reset WDT, can be used to delay WDT reset
         */
        void reset();
        
};

/** 
 * sleep modes available on the Feather M4 Express. 
 */
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

/**
 * object for putting MCU to sleep
 */
class SleepController
{
    private:

    public:

        /**
         * constructor for SleepController
         */
        SleepController();
        /**
         * put MCU to sleep in specified sleep mode
         * @param sleepMode mode which to put MCU to sleep in
         * @see SLEEPMODES
         */
        void goToSleep(SLEEPMODES sleepMode);

};

/**
 * Wrapper class for SD.h
 */
class SDWrapper
{
    private:

        uint8_t PIN_CS; ///< chip select pin for SD
        
    public:

        File data;      ///< file descriptor, files opened by this wrapper class will use this descriptor

        /**
         * constructor for SDWrapper
         * @param PIN_CS chip select pin
         */
        SDWrapper(uint8_t PIN_CS);

        /**
         * attempts to access SD card
         * @return true on success
         */
        bool initialize(void);

        /**
         * starts SD communication
         * @return true on success
         */
        bool begin(void);

        /**
         * ends SD communication
         */
        void end(void);

        /**
         * opens a file in specified mode
         * @param filename char array containing directory of file
         * @param mode mode in which the file should be open
         * @return true on success
         */
        bool openFile(char *filename, uint8_t mode);

        /**
         * closes file descriptor
         */
        void closeFile(void);

};

/**
 * Wrapper for Adafruit_VC0706
 */
class TTLCamera
{
    private:

    public:
        
        Adafruit_VC0706 *cam = NULL;    ///< pointer to Adafruit_VC0706 object

        /**
         * constructor for TTLCamera
         */
        TTLCamera();
        
        /**
         * ensures that the TTLCamera can be accessed
         * @return true on success
         */
        bool initialize(void);

        /**
         * takes a photo and saves it to a file on SD card
         * @param file pointer to file descriptor
         * @return true on success
         */
        bool takePhoto(File *file);

};

/**
 * Wrapper class for RTC_DS3213
 */
class RTCWrapper
{
    private:

        uint8_t i2c_address;    ///< i2c address

    public:

        RTC_DS3231 *rtc = NULL; ///< pointer to RTC_DS3213 object

        /**
         * constructor for RTCWrapper
         * @param i2c_address i2c address of DS3213
         */
        RTCWrapper(uint8_t i2c_address);

        /**
         * attempt to communicate with DS3213, check if power has been lost, set rtc in alarm mode and reset alarms
         * @return true on success
         */
        bool initialize(void);

        /**
         * get the current date and time from RTC
         * @return DateTime object
         */
        DateTime getDateTime(void);

        /**
         * clear alarm 1 on DS3213
         */
        void clearAlarm(void);
        
        /**
         * set alarm to some DateTime in the future
         * @param alarmDateTime DateTime object
         * @return true on success
         */
        bool setAlarm(DateTime alarmDateTime);

        /**
         * set alarm some seconds in the future
         * @param seconds number of seconds from current DateTime
         * @return true on success
         */
        bool setAlarmSeconds(int32_t seconds);

};

/**
 * class for MCP465 digital potentiometer
 */
class MCP465
{
    private:

        uint8_t i2c_address;    ///< i2c address of MCP465

    public:

        /**
         * constructor for MCP465
         * @param i2c_address i2c address of chip
         */
        MCP465(uint8_t i2c_address);

        /**
         * attempts to communicate with chip
         * @return true on success
         */
        bool initialize(void);

        /**
         * increment wiper register value
         * @return 0 on success
         */
        uint8_t incrementWiper(void);
        /**
         * decrement wiper register value
         * @return 0 on success
         */
        uint8_t decrementWiper(void);

        /**
         * write a value to wiper register
         * @param wiperValue wiper value [0, 256]
         * @return 0 on success
         */
        uint8_t writeWiperValue(uint16_t wiperValue);
        /**
         * reads value of wiper register
         * @return wiper register value, -1 on error
         */
        int16_t readWiperValue(void);

};

/**
 * class for PAM8302 amplifier
 */
class PAM8302
{
    private:

        uint8_t PIN_SD; ///< shutdown pin
    
    public:

        /**
         * constructor for PAM8302
         */
        PAM8302(uint8_t PIN_SD);

        /**
         * disables amplifier shutdown
         */
        void powerOn();
        /**
         * enables amplifier shutdown
         */
        void powerOff();
};

#endif
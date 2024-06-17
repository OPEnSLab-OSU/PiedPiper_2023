/**
 * @file PiedPiperStripped.ino
 * 
 * @mainpage Pied Piper Stripped Software 
 *
 * @section description Description
 * This code is intended for stripped down version of Pied Piper which performs intermittent playbacks based on operation intervals
 * that are stored on the SD card. The code performs in the following manner:
 * 1. In setup(): all necassary data is loaded from the SD card, filenames for playback sound and playback intervals file are loaded
 *    first from the ("settings.txt") file on SD card. Following this, the playback sound (*.PAD) and playback intervals (*.txt) are 
 *    loaded.  If all data is successfully loaded from the SD card and RTC is initialized device flashes NeoPixel green to indicate 
 *    success. Otherwise, NeoPixel flashes red and device resets in a in about 2 seconds using WDT. 
 *    On success device perfroms a setup playback and the RTC alarm is set to reset the device when the alarm goes off based on the 
 *    following conditions:
 *    - The current time is within the time of a playback interval - the alarm is set to fire at the time corresponding 
 *      to the end of the current playback interval (i.e: if playback interval is between 5:00 and 6:00, the alarm will fire
 *      at 6:00). The device will perform the tasks assigned in loop() until the RTC alarm goes off.
 *    - The current time is not is not within a playback interval - the alarm is set to fire at the time corresponding to the
 *      next available operation interval (i.e. if the next available playback interval is between 7:00 and 8:00, the alarm will
 *      fire at 7:00). The device will sleep in the "OFF" mode until RTC alarm goes off.
 * 2. In loop(): At this moment Playback() is called continously which continously performs playback, but more tasks can be added in
 *    future versions.
 *
 * @section libraries Libraries
 * - SD, SPI, Wire, RTCLib
 *  - used for reading and writing data to SD card, and interacting with RTC
 * - SAMDTimerInterrupt
 *  - timer interrupt library for SAMD boards. Used for performing playback
 * - Adafruit_NeoPixel
 *  - Used for indicating initialization errors
 *
 * @section todo TODO
 * - Add possibility to set delay between playbacks
 * - Add more tasks for device to perform in loop (if requested)
 *
 * @section author Author
 *  - Created by Mykyta (Nick) Synytsia and Vincent Vaughn (June 16, 2024)
 *
 * Copyright (c) 2024 OSU OPEnSLab. All rights reserved.
 */

// Libraries
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include "RTClib.h"
#include "SAMDTimerInterrupt.h"
#include <Adafruit_NeoPixel.h>

// Defines
#define AUD_OUT_SAMPLE_FREQ 4096  ///< original sample rate of playback file
#define AUD_OUT_TIME 8  ///< maximum length of playback file in seconds

#define SINC_FILTER_UPSAMPLE_RATIO 8 ///< sinc filter upsample ratio, ideally should be a power of 2
#define SINC_FILTER_ZERO_X 5    ///< sinc filter number of zero crossings, more crossings will produce a cleaner result but will also use more processor time 

#define SD_OPEN_ATTEMPT_COUNT 1  ///< number of times to retry SD.begin()
#define SD_OPEN_RETRY_DELAY_MS 100  ///< delay between SD.begin() attempts

#define AUD_OUT A0    ///< Audio output pin

#define HYPNOS_5VR 6  ///< Hypnos 5 volt rail
#define HYPNOS_3VR 5  ///< Hypnos 3 volt rail

#define SD_CS 11 ///< Chip select for SD
#define AMP_SD 9  ///< Amplifier enable

#define DEFAULT_PLAYBACK_INT 1440 ///< Briefly wake device after this many minutes if no playback intervals are defined (PBINT.txt is empty) If set to 0, playback will be performed forever and operation times defined in PBINT.txt will be ignored.

const char settings_filename[] = "settings.txt"; ///< Name of file to load settings from such as name of playback and operation intervals filenames

// Types

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

/** data structure for storing operation times parsed from the PBINT.txt file on SD card. */
struct opTime {
  uint8_t hour;   ///< the corresponding hour
  uint8_t minute; ///< the corresponding minute
  int16_t minutes;  ///< total minutes (hour * 60 + minute)
};

// Global Constants
const SLEEPMODES sleepmode = OFF; ///< For this version, "OFF" sleep mode is used which results in the least power consumption (draws less than 1mA)

const int outputSampleDelayTime = 1000000 / (AUD_OUT_SAMPLE_FREQ * SINC_FILTER_UPSAMPLE_RATIO); ///< output sample delay time, essentially how often the timer interrupt will fire (in microseconds)

const int sincTableSizeUp = (2 * SINC_FILTER_ZERO_X + 1) * SINC_FILTER_UPSAMPLE_RATIO - SINC_FILTER_UPSAMPLE_RATIO + 1; ///< size of sinc filter table

// Global Variables
uint16_t outputSampleBuffer[AUD_OUT_SAMPLE_FREQ * AUD_OUT_TIME]; ///< output sample buffer stores the values corresponding to the playback file that is loaded from the SD card
volatile int outputSampleBufferIdx = 0; ///< stores the position of the current sample for playback
volatile uint16_t nextOutputSample = 2048; ///< stores value of the next output sample for playing back audio

int playbackSampleCount = AUD_OUT_SAMPLE_FREQ * AUD_OUT_TIME; ///< Stores the total number of sample loaded from the playback file. Originally this is set to the maximum number of samples that can be stored SAMPLE_RATE * AUD_OUT_TIME, but this value is updated upon call to LoadSound() since our playback sound isn't garanteed to be exactly AUD_OUT_TIME seconds long

float sincFilterTableUpsample[sincTableSizeUp]; ///< stores values of sinc function (sin(x) / x) for upsampling the samples stored in outputSampleBuffer during Playback() using outputUpsampledSample()

volatile uint16_t upsampleFilterInput[sincTableSizeUp]; ///< sinc filter input buffer for upsampling
volatile int upsampleInputIdx = 0;  ///< next position in upsample input buffer
volatile int upsampleInputCount = 0;  ///< upsample count, determines whether to pad with zeroes or read next value from outputSampleBuffer

opTime* operationTimes = NULL; ///< *operationTimes is a dynamically allocated array which stores parsed operation times from PBINT.txt, dynamic memory is used for this because we do not know how many operation times are defined in PBINT.txt during compilation
int numOperationTimes = 0; ///< the number of elements in operationTimes array

char playback_filename[32]; ///< name of the file used for playback audio, name of file is loaded from SD card
char playback_interval[32]; ///< name of the file used for loading operation intervals, name of file is loaded from SD card

Adafruit_NeoPixel indicator(1, 8, NEO_GRB + NEO_KHZ800); ///< LED indicator

File data;  ///< used for reading files from SD

RTC_DS3231 rtc; ///< object for interacting with rtc

SAMDTimer ITimer(TIMER_TC3); ///< SAMD timer interrupt 

/**
 * Disable timer interrupt
 *
 * This is used to disable the interrupt caused by timer
 */
void stopISRTimer()
{
  ITimer.detachInterrupt();
  ITimer.restartTimer();
}

/**
 * Enable timer interrupt
 *
 * @param interval_us   The interval between each alarm in microseconds
 * @param fnPtr         Pointer to callback function
 */
void startISRTimer(const unsigned long interval_us, void(*fnPtr)())
{
  ITimer.attachInterruptInterval(interval_us, fnPtr);
}

/**
 * Initialization routine
 *
 * Ensures SD, RTC can and are properly initialized. If so, playback sound and operation times are loaded based on filenames defined in
 * "settings.txt" on SD card, if a failure is detected, a NeoPixel flashes red indefinetely. Otherwise, on success, NeoPixel flashes green,
 * and setup playback is performed. Finally, device determines whether to go to sleep or perform tasks that are in loop().
 */
void setup() {
  // set LED to yellow upon start of initialization...
  indicator.begin();
  indicator.clear();

  // begin Serial, and ensure that our analogWrite() resolution is set to the maximum resolution (2^12 == 4096)
  Serial.begin(2000000);
  analogWriteResolution(12);

  delay(4000); // this delay is not needed, however it provides some time to open Serial monitor for debugging

  Serial.println("\nInitializing...");

  // configure pin modes
  configurePins();

  Serial.println("Initializing SD...");

  // turn on 3VR rail
  digitalWrite(HYPNOS_3VR, LOW);

  ResetSPI();

  // Verify that SD can be initialized; stop the program if it can't.
  if (!SD.begin(SD_CS)) {
    digitalWrite(HYPNOS_3VR, HIGH);
    Serial.println("SD failed to initialize!");
    initializationFailFlash();
  }

  // loads settings file from SD card. Specifying which file to use for playback and for operation intervals
  if (!LoadSettings()) {
    digitalWrite(HYPNOS_3VR, HIGH);
    Serial.println("Load settings from SD failed!");
    initializationFailFlash();
  }
  
  // loads, parses, and stores operation times from "PBINT.txt" file
  // on SD card to dynamically allocated array: opTimes *operationTimes
  // returns False upon failure and initiates fail flash
  if (!LoadOperationTimes()) {
    digitalWrite(HYPNOS_3VR, HIGH);
    Serial.println("Reading operation times from SD failed!");
    initializationFailFlash();
  }

  // load playback sound from SD card and stores values to outputSampleBuffer
  // returns False upon failure and initiates fail flash
  if (!LoadSound()) {
    digitalWrite(HYPNOS_3VR, HIGH);
    Serial.println("Reading playback sound from SD failed!");
    initializationFailFlash();
  }

  Serial.println("Initializing RTC");

  // begin i2c
  Wire.begin();

  // attempt to initialize RTC
  if (!initializeRTC()) {
    Wire.end();
    digitalWrite(HYPNOS_3VR, HIGH);
    Serial.println("RTC failed to initialize!");
    initializationFailFlash();
  } else {
  // writes RTC time to SD card to keep track of how long trap has been active (This may be used to adjust RTC time)
  LogAlive();
  }

  // end i2c
  Wire.end();

  // end SD communication
  SD.end();

  // power off rails
  digitalWrite(HYPNOS_3VR, HIGH);

  // calculate sinc filter table for upsampling values stored in outputSampleBuffer
  calculateUpsampleSincFilterTable();

  // flash LED green to indicate that initialization was successful
  initializationSuccessFlash();

  // enable and disable isr timer for initial setup of timer interrupt
  startISRTimer(outputSampleDelayTime, emptyFunction);
  stopISRTimer();

  // perform setup playback to confirm that Playback() is working (just a precaution, can be commented out)
  Serial.println("Performing setup playback...");
  Playback();

  Serial.println("Setup complete");

  Serial.println();

  // setupOperation() returns true if the RTC time is within an operation interval,
  // otherwise, returns false. This is also where RTC alarm is set based on time.
  if (!setupOperation()) {
    goToSleep(sleepmode);
  }
}

/**
 * Tasks that are to be performed when the device is awake
 *
 * At this time the only task that is performed is Playback(), but more can be added in future revision
 */ 
void loop() {
  Playback();
  // may add option to add delay between playbacks in the future (will likely be set in PBINT.txt)
}

/**
 * Initialize RTC and set to alarm mode
 *
 * @return true if successful
 */
bool initializeRTC() {
  // Initialize I2C communication
  // digitalWrite(HYPNOS_3VR, LOW);
  // Wire.begin();

  // Initialize RTC
  if (!rtc.begin()) {
    return false;
  }

  // Check if RTC lost power; adjust the clock to the compilation time if it did.
  // Ideally, the RTC will adjust to the time based on the actual time of day, rather than the compilation time of the sketch
  // Right here, it may be wise to configure device to just do playbacks every 5 minutes or something...
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, check voltage on HYPNOS battery. RTC should be properly adjusted!");
    return false;
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // reset rtc alarms
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);

  // set RTC !INT/SQR pin to interrupt mode
  rtc.writeSqwPinMode(DS3231_OFF);

  // disable alarm 2 since we are not using it
  rtc.disableAlarm(2);

  // RTC initialization done, end wire and turn off 3v rail
  // Wire.end();
  // digitalWrite(HYPNOS_3VR, HIGH);

  return true;
}

/**
 * Configures pin modes for all pins
 */
void configurePins() {
  pinMode(AUD_OUT, OUTPUT);
  pinMode(HYPNOS_5VR, OUTPUT);
  pinMode(HYPNOS_3VR, OUTPUT);
  pinMode(SD_CS, OUTPUT);  
  pinMode(AMP_SD, OUTPUT);
}

/**
 * Puts device to sleep in selected sleep mode
 *
 * Sleep modes such as IDLE through STANDBY return to the instruction after __WFI(), in this case the return address of the intruction following goToSleep()
 * However, when going to sleep in HIBRATE though OFF mode, the only way to exit is by resetting the board (At least on the Feather M4 Express)
 *
 * @param mode  the sleep mode to use
 */
void goToSleep(SLEEPMODES mode) {
  Serial.println("Going to sleep");

  // end USB Serial, probably can be commented out
  USBDevice.detach();
  USBDevice.end();

  // ensure that the correct sleep mode is set
  PM->SLEEPCFG.bit.SLEEPMODE = mode;
  while(PM->SLEEPCFG.bit.SLEEPMODE != mode);

  // go to sleep by ensuring all memory accesses are complete with (__DSB) and calling wait for interrupt (__WFI)
  __DSB(); // since we are using "OFF" sleep mode, this may not be necassary
  __WFI();
}

/**
 * open a file in some mode (read, write, etc), using file descriptor "data". File is created if mode == FILE_READ
 *
 * @note Does not call SD.begin() or control hypnos rails. Up to user to call prior to using this function.
 * @param fname name of file.
 * @param mode open file in this mode (i.e: FILE_READ).
 * @return True on success.
 */ 
bool OpenFile(char *fname, uint8_t mode) {
  // check if file exists (also check only if mode == FILE_READ)
  if (mode == FILE_READ && !SD.exists(fname)) return false;

  // open file in mode
  data = SD.open(fname, mode);

  // check if file was opened successfully
  if (!data) return false;

  // return true on success
  return true;
}

/**
 * load trap settings from "trap.settings". At this time these settings include which file to use for playback and for operation times
 *
 * @return True on success
 */
bool LoadSettings() {
  // ResetSPI();

  // if (!SD.begin(SD_CS)) {
  //   digitalWrite(HYPNOS_3VR, HIGH);
  //   return false;
  // }

  if (!OpenFile((char *)settings_filename, FILE_READ)) {
    digitalWrite(HYPNOS_3VR, HIGH);
    return false;
  }

  while (data.available()) {
    String settingName = data.readStringUntil(':');
    data.read();
    String setting = data.readStringUntil('\n');
    setting.trim();

    // store to corresponding setting on device
    if (settingName == "playback_filename") {
      strcpy(playback_filename, "/PBAUD/");
      strcat(playback_filename, setting.c_str());
      
    } else if (settingName == "playback_interval") {
      strcpy(playback_interval, "/PBINT/");
      strcat(playback_interval, setting.c_str());
    } else {
      continue;
    }
  }

  data.close();

  // SD.end();

  return true;
}

/**
 * load and store operation times from "PBINT.txt" to dynamically allocated operation times array.
 *
 * @return True on success
 */
bool LoadOperationTimes() {
  // begin communication with SD
  // ResetSPI();

  // digitalWrite(HYPNOS_3VR, LOW);

  // if (!SD.begin(SD_CS)) {
  //   digitalWrite(HYPNOS_3VR, HIGH);
  //   return false;
  // }

  if (!OpenFile(playback_interval, FILE_READ)) {
    digitalWrite(HYPNOS_3VR, HIGH);
    return false;
  }

  uint8_t hour = 0;
  uint8_t minute = 0;
  // read and store playback intervals
  while(data.available()) {
    // data in operation intervals file is stored in the following manner (XX:XX - XX:XX)
    // read hour from file (read until colon)
    hour = data.readStringUntil(':').toInt();
    // read minute
    minute = data.readStringUntil(' ').toInt();
    addOperationTime(hour, minute);
    // read until start of end time
    data.readStringUntil(' ');
    // read hour and minute
    hour = data.readStringUntil(':').toInt();
    minute = data.readStringUntil('\n').toInt();
    addOperationTime(hour, minute);
  }

  data.close();
  
  // SD.end();
  // digitalWrite(HYPNOS_3VR, HIGH);

  // print playback intervals
  Serial.println("Printing operation hours...");
  for (int i = 0; i < numOperationTimes; i = i + 2) {
    Serial.printf("\t%02d:%02d - %02d:%02d\n", operationTimes[i].hour, operationTimes[i].minute, operationTimes[i + 1].hour, operationTimes[i + 1].minute);
  }
  Serial.println();

  return true;
}

/**
 * Loads playback sound file from the SD card and stores to outputSampleBuffer as samples
 * 
 * Once loaded, the data will remain in-place until LoadSound() is called again. This means
 * that all future calls to Playback() will play back the same audio data.
 *
 * @return True if playback sound was loaded successfully
 */
bool LoadSound() {
  // begin communication with SD
  // ResetSPI();
  // digitalWrite(HYPNOS_3VR, LOW);

  // if (!SD.begin(SD_CS)) {
  //   digitalWrite(HYPNOS_3VR, HIGH);
  //   return false;
  // }

  if (!OpenFile(playback_filename, FILE_READ)) {
    // digitalWrite(HYPNOS_3VR, HIGH);
    return false;
  }

  //Serial.println("Opened file. Loading samples...");

  // get size of playback file (in bytes)
  int fsize = data.size();

  // playback file is exported as 16-bit unsigned int, the total number of samples stored in the file is fsize / 2
  playbackSampleCount = fsize / 2;
  
  for (int i = 0; i < playbackSampleCount; i++) {
    // stop reading at end of file, and ensure 2 bytes are available for reading
    if ((2 * i) >= (fsize - 1)) break;
    // read two bytes at a time and store to outputSampleBuffer[i]
    data.read(&outputSampleBuffer[i], 2);
  }

  data.close();
  // SD.end();

  // digitalWrite(HYPNOS_3VR, HIGH);

  return true;
}

/**
 * allocate memory to store operation time into dynamically allocated array operationTimes, this function is intended to be used by LoadOperationTimes().
 *
 * @param hour  the hour
 * @param minute the minute
 */
void addOperationTime(int hour, int minute) {
  // increment size of operatimesTimes array
  numOperationTimes += 1;
  // reallocate memory for new array size
  operationTimes = (opTime*)realloc(operationTimes, numOperationTimes * sizeof(opTime));
  // store data
  operationTimes[numOperationTimes - 1].hour = hour;
  operationTimes[numOperationTimes - 1].minute = minute;
  operationTimes[numOperationTimes - 1].minutes = hour * 60 + minute;
}


/**
 * Set RTC alarm to corresponding operation time based on the time of the RTC. 
 *
 * If RTC time is within an operation interval, the device should be awake, alarm is set the the end of that interval.
 * Otherwise, RTC is set to the start of the next operation true. 
 * If no operation times are defined in PBINT.txt, alarm will occur every @DEFAULT_PLAYBACK_INT minutes.
 *
 * @return True if RTC time is within an operation interval (indicating that device should be awake)
 */
bool setupOperation() {
  // perform playback continously if DEFAULT_PLAYBACK_INT is 0
  if (DEFAULT_PLAYBACK_INT == 0) return 1;

  // get the current date and time from RTC
  digitalWrite(HYPNOS_3VR, LOW);
  Wire.begin();

  // Serial.printf("The current temperature is: %.1f\n", rtc.getTemperature());
  // getting the current date and time from RTC
  DateTime nowDT = rtc.now();
  // at this point, setup() should have cleared alarm since "OFF" mode is used, but in other sleep modes such as STANDBY
  // where IP returns to instruction after __WFI, it would make sense to clear the alarm prior to setting one
  rtc.clearAlarm(1);

  Wire.end();
  digitalWrite(HYPNOS_3VR, HIGH);

  // convert the current time to minutes (0 - 1440)
  int nowMinutes = nowDT.hour() * 60 + nowDT.minute();
  int nowSecond = nowDT.second();

  // stores the difference between current time and next alarm time for setting RTC alarm
  int nextAlarmTime = -1;

  // final return value, indicating whether or not playback should be performed
  bool performPlayback = 0;

  // Device will wake once every DEFAULT_PLAYBACK_INT minutes if no operation times are defined
  if (numOperationTimes == 0) {
    nextAlarmTime = DEFAULT_PLAYBACK_INT;
  // Otherwise, alarm will be set based on the current operation interval
  } else {
    // check if RTC time is within time of operation interval
    for (int i = 0; i < numOperationTimes - 1; i += 2) {
      //Serial.printf("Now: %d, Start: %d, End: %d\n", nowMinutes, operationTimes[i].minutes, operationTimes[i + 1].minutes);
      if (nowMinutes >= operationTimes[i].minutes && nowMinutes < operationTimes[i + 1].minutes) {
        Serial.println("Performing playback...");
        // if within an operation interval, then playback will be performed
        performPlayback = 1;
        // the next alarm time is the difference between the end of this operation time and the current time (in minutes)
        nextAlarmTime = operationTimes[i + 1].minutes - nowMinutes;
        break;
      }
    }

    // if the current time is not within an operation time then set nextAlarmTime to the next available operation time
    if (!performPlayback) {
      for (int i = 0; i < numOperationTimes - 1; i += 2) {
        // the next available operation time is determined when the start of an operation interval exceeds the current time (in minutes)
        if (operationTimes[i].minutes > nowMinutes) {
          // the next alarm time is the difference between the start of the next operation interval and the current time
          nextAlarmTime = operationTimes[i].minutes - nowMinutes;
          break;
        }
      }
    }

    // if nextAlarmTime wasn't set (if no operation times exist after the current time), then set to first operation time (24 hour wrap)
    if (nextAlarmTime == -1) {
      // next alarm time is the difference between the current time and the first available operation time
      nextAlarmTime = 1440 - nowMinutes + operationTimes[0].minutes;
    }
  }

  // prints when the next alarm will occur
  if (nextAlarmTime < 60) Serial.printf("Next alarm will happen in %d minutes", nextAlarmTime);
  else  Serial.printf("Next alarm will happen in %d hours and %d minutes", nextAlarmTime / 60, nextAlarmTime % 60);
  Serial.println();

  // schedule RTC alarm nextAlarmTime minutes into the future
  digitalWrite(HYPNOS_3VR, LOW);
  Wire.begin();

  // ensure RTC alarm gets set, rtc.setAlarm1() returns True if alarm was set.
  while (!rtc.setAlarm1(
      // TimeSpan() accepts seconds as argument. Therefore convert nextAlarmTime to seconds by multiplying by 60
      // subtracting the current seconds isn't necassary, but helps to ensure that alarm goes of exactly at the minute mark 
      nowDT + TimeSpan(nextAlarmTime * 60 - nowSecond),
      DS3231_A1_Hour // this mode triggers the alarm when the hours, minutes and seconds match
  )) {
    Serial.println("ERROR: alarm was not set!");
  }

  Wire.end();
  digitalWrite(HYPNOS_3VR, HIGH);

  // returns true if RTC time is within an operation interval
  return performPlayback;
}

/**
 * Plays back the playback sound that is loaded from SD card 
 *
 * This works by using the timer interrupt, which is triggered at @SINC_FILTER_UPSAMPLE_RATIO * @SAMPLE_RATE Hz.
 * Although the playback sound was stored at sample rate of @SAMPLE_RATE Hz, a digital sinc filter is used to upsample this data in realtime to the necassary sample rate.
 */
void Playback() {
  stopISRTimer();

  //Serial.println("Enabling amplifier...");

  digitalWrite(HYPNOS_5VR, HIGH); // enable 5VR
  analogWrite(AUD_OUT, 2048);     // write 2048 to AUD_OUT (3.3V / 2)
  digitalWrite(AMP_SD, HIGH);     // enable amplifier
  delay(1000);                     // short delay for analog low pass filter

  //Serial.println("Amplifier enabled. Beginning playback ISR...");

  // begin outputting samples using timer interrupt
  startISRTimer(outputSampleDelayTime, OutputUpsampledSample);

  // wait for full duration of playback sound to be played
  while (outputSampleBufferIdx < playbackSampleCount)
    ;

  // stop timer interrupt
  stopISRTimer();

  // restore output buffer index
  outputSampleBufferIdx = 0;

  //Serial.println("Finished playback ISR.\nShutting down amplifier...");

  digitalWrite(AMP_SD, LOW);    // disable amplifier
  analogWrite(AUD_OUT, 0);      // set AUD_OUT to 0V
  digitalWrite(HYPNOS_5VR, LOW);  // disable 5VR

  //Serial.println("Amplifer shut down.");
}

/**
 * Blank function for ISR setup
 */
void emptyFunction() {
  return;
}

/**
 * writes a sample to AUD_OUT and calculates next sample using sinc filter, this function is called by timer interrupt.
 */
void OutputUpsampledSample() {
  // write sample to AUD_OUT
  analogWrite(AUD_OUT, nextOutputSample);

  // return if outside of outputSampleBuffer bounds
  if (outputSampleBufferIdx == playbackSampleCount) return;
  // Otherwise, calculate next upsampled value for AUD_OUT

  // store last location of upsampling input buffer
  int upsampleInputIdxCpy = upsampleInputIdx;
  
  // store value of sample to filter input buffer when upsample count == 0, otherwise pad with zeroes
  upsampleFilterInput[upsampleInputIdx++] = upsampleInputCount++ > 0 ? 0 : outputSampleBuffer[outputSampleBufferIdx++];
  
  if (upsampleInputCount == SINC_FILTER_UPSAMPLE_RATIO) upsampleInputCount = 0;
  if (upsampleInputIdx == sincTableSizeUp) upsampleInputIdx = 0;

  // calculate upsampled value
  float filteredValue = 0.0;
  // convolute filter input with sinc function
  for (int i = 0; i < sincTableSizeUp; i++) {
    filteredValue += upsampleFilterInput[upsampleInputIdxCpy++] * sincFilterTableUpsample[i];
    if (upsampleInputIdxCpy == sincTableSizeUp) upsampleInputIdxCpy = 0;
  }

  // copy filtered value to next output sample
  nextOutputSample = max(0, min(4095, round(filteredValue)));
}


/**
 * Calculates sinc filter table for upsampling a signal by @SINC_FILTER_UPSAMPLE_RATIO
 */
void calculateUpsampleSincFilterTable() {
  int ratio = SINC_FILTER_UPSAMPLE_RATIO;
  int nz = SINC_FILTER_ZERO_X;
  // Build sinc function for upsampling by ratio with 
  int n = sincTableSizeUp;

  // stores time values corresponding to sinc function
  float ns[n];
  float ns_step = float(nz * 2) / (n - 1);

  // stores time values corresponding to cosine function for windowing the sinc function
  float t[n];
  float t_step = 1.0 / (n - 1);

  for (int i = 0; i < n; i++) {
    // calculate time values for sinc function, [-nz to nz] spaced apart by ns_step
    ns[i] = float(-1.0 * nz) + ns_step * i;
    // calculate time values for cosine function, [0.0 to 1.0] spaced apart by t_step
    t[i] = t_step * i;
  }

  // ensure to not divide by 0
  ns[round((n - 1) / 2.0)] = 1.0;

  // calculate sinc function and store in table
  for (int i = 0; i < n; i++) {
    sincFilterTableUpsample[i] = sin(PI * ns[i]) / (PI * ns[i]); 
  }
  
  // sinc function is 'undefined' at 0 (sinc(0)/0), therefore set to 1.0
  sincFilterTableUpsample[int(round((n - 1) / 2.0))] = 1.0;

  // window sinc function table with cosine wave 
  for (int i = 0; i < n; i++) {
    sincFilterTableUpsample[i] = sincFilterTableUpsample[i] * 0.5 * (1.0 - cos(2.0 * PI * t[i]));
  }
}

/**
 * Reset SPI pins, called prior to SPI.begin() as a precaution
 */
void ResetSPI() {
  pinMode(23, INPUT);
  pinMode(24, INPUT);
  pinMode(10, INPUT);

  delay(20);

  pinMode(23, OUTPUT);
  pinMode(24, OUTPUT);
  pinMode(10, OUTPUT);
}

/**
 * Attempts to initialize SD card on Hypnos
 *
 * @return True on successful initialization
 */
bool BeginSD() {
  for (int i = 0; i < SD_OPEN_ATTEMPT_COUNT; i++) {
    if (SD.begin(SD_CS)) return true;
    //Serial.print("SD failed to initialize in SaveDetection on try #");
    //Serial.println(i);
    delay(SD_OPEN_RETRY_DELAY_MS);
  }

  return false;
}

/**
 * Log the current time to gauge how long the device has been alive in the field
 */
void LogAlive() {
  // ResetSPI();
  // digitalWrite(HYPNOS_3VR, LOW);

  // if (!SD.begin(SD_CS)) {
  //   digitalWrite(HYPNOS_3VR, HIGH);
  //   return;
  // }

  // open file for writing, OpenFile will create LOG.txt if it does not exist
  if (!OpenFile("LOG.txt", FILE_WRITE)) {
    return;
  }

  char date[] = "YYMMDD-hh:mm:ss";

  // Wire.begin();
  rtc.now().toString(date);
  // Wire.end();
  
  data.println(date);

  Serial.println(date);
  
  data.close();

  // SD.end();
  // digitalWrite(HYPNOS_3VR, HIGH);
}

/**
 * Flashes LED to indicate initialization error
 */
void initializationFailFlash() {
  // reset board after 4096 cycles with WDT  
  REG_WDT_CONFIG = WDT_CONFIG_PER_CYC4096;
  REG_WDT_CTRLA = WDT_CTRLA_ENABLE;

  // flash NeoPixel red
  while(1) {
    indicator.setPixelColor(0, 255, 0, 0);
    indicator.show();
    delay(500);
    indicator.setPixelColor(0, 0, 0, 0);
    indicator.show();
    delay(500);
  }
}

/**
 * Flashes LED green to indicate initialization success
 */
void initializationSuccessFlash() {
  // flash NeoPixel green
  delay(500);
  indicator.setPixelColor(0, 0, 255, 0);
  indicator.show();
  delay(500);
  indicator.setPixelColor(0, 0, 0, 0);
  indicator.show();
  delay(500);
  indicator.clear();
}
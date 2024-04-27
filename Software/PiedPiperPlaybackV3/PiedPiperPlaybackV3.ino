/*
  This code is for stripped down version of Pied Piper which performs intermittent playbacks based on operation intervals
  stored on the SD card. The code performs in the following manner:
    1. In setup(): all necassary data is loaded from the SD card, such as the playback sound (*.PAD) and playback intervals 
        (PBINT.txt). If all data is successfully loaded from the SD card and RTC is initialized, then RTC alarm is set to reset
        the device when the alarm goes off. The alarm is set based on the following conditions:
          a. The current time is within the time of a playback interval - the alarm is set to fire at the time corresponding 
              to the end of the current playback interval (i.e: if playback interval is between 5:00 and 6:00, the alarm will fire
              at 6:00). The device will perform the tasks assigned in loop() until the RTC alarm goes off.
          b. The current time is not is not within a playback interval - the alarm is set to fire at the time corresponding to the
              next available operation interval (i.e. if the next available playback interval is between 7:00 and 8:00, the alarm will
              fire at 7:00). The device will sleep in the "OFF" mode until RTC alarm goes off.
    2. In loop(): At this moment Playback() is called continously which continously performs playback, but more tasks can be added in
        future versions.
*/

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include "RTClib.h"
#include "SAMDTimerInterrupt.h"

// name of the file used for playback
const char playback_filename[] = "BMSB.PAD";

#define AUD_OUT_SAMPLE_FREQ 4096
#define AUD_OUT_TIME 8  // maximum length of playback file in seconds

#define AUD_OUT_UPSAMPLE_RATIO 8 // sinc filter upsample ratio, ideally should be a power of 2
#define AUD_OUT_UPSAMPLE_FILTER_SIZE 7 // // upsample sinc filter number of zero crossings, more crossings will produce a cleaner result but will also use more processor time 

#define SD_OPEN_ATTEMPT_COUNT 10  // number of times to retry SD.begin()
#define SD_OPEN_RETRY_DELAY_MS 100  // delay between SD.begin() attempt

#define AUD_OUT A0    // Audio output pin

#define HYPNOS_5VR 6
#define HYPNOS_3VR 5

#define SD_CS 11 // Chip select for SD
#define AMP_SD 9  // Amplifier enable

#define DEFAULT_PLAYBACK_INT 1440 // Briefly wake device after this many minutes if no playback intervals are defined (PBINT.txt is empty)
                                  // If set to 0, playback will be performed forever and operation times defined in PBINT.txt will be ignored.

// various sleep modes available on the Feather M4 Express
enum SLEEPMODES
{
  IDLE0 = 0x0,
  IDLE1 = 0x1,
  IDLE2 = 0x2,
  STANDBY = 0x4,    // STANDBY sleep mode can exited with a interrupt (this sleep mode is good, but draws ~20mA by default)
  HIBERNATE = 0x5,  // HIBERNATE sleep mode can only be exited with a device reset
  BACKUP = 0x6,     // BACKUP sleep mode can only be exited with a device reset
  OFF = 0x7         // OFF sleep mode can only be exited with a device reset (best sleep mode for power consumption)
};

// For this version, we are using the OFF sleep mode which results in the least power consumption (draws less than 1mA)
SLEEPMODES sleepmode = OFF;

// Audio output stuff

// output sample delay time, essentially how often the timer interrupt will fire (in microseconds)
const int outputSampleDelayTime = 1000000 / (AUD_OUT_SAMPLE_FREQ * AUD_OUT_UPSAMPLE_RATIO);

// output sample buffer stores the values corresponding to the playback file that is loaded from the SD card
short outputSampleBuffer[AUD_OUT_SAMPLE_FREQ * AUD_OUT_TIME];
volatile int outputSampleBufferPtr = 0; // stores the position of the current sample for playback

// stores the total number of samples loaded from the playback file. 
// Originally, this is set to the maximum number of samples that can be stored (SAMPLE_RATE * AUD_OUT_TIME),
// but since our playback file will not always be exactly AUD_OUT_TIME seconds, this value
// will change when LoadSound() is called.
int playbackSampleCount = AUD_OUT_SAMPLE_FREQ * AUD_OUT_TIME;

// stores the value of the next output sample for analogWrite()
volatile int nextOutputSample = 0;

// stores values of sinc function (sin(x) / x) for upsampling the samples stored in outputSampleBuffer during Playback()
const int sincTableSizeUp = (2 * AUD_OUT_UPSAMPLE_FILTER_SIZE + 1) * AUD_OUT_UPSAMPLE_RATIO - AUD_OUT_UPSAMPLE_RATIO + 1;
float sincFilterTableUpsample[sincTableSizeUp];

// circular input buffer for upsampling
volatile short upsampleInput[sincTableSizeUp];
volatile int upsampleInputPtr = 0;  // next position in upsample input buffer
volatile int upsampleInputC = 0;  // upsample count

/* used for reading and writing data */
File data;

/* object for interacting with rtc */
RTC_DS3231 rtc;

/* SAMD timer interrupt */
SAMDTimer ITimer(TIMER_TC3);

/* Functions to start and stop the ISR timer */
void stopISRTimer()
{
  ITimer.detachInterrupt();
  ITimer.restartTimer();
}

void startISRTimer(const unsigned long interval_us, void(*fnPtr)())
{
  ITimer.attachInterruptInterval(interval_us, fnPtr);
}

/* Variables related to storing data read from operation intervals file (PBINT.txt) */

// data structure for storing operation times parsed from the PBINT.txt file on SD card
struct opTime {
  short hour;
  short minute;
  int minutes;
};

// *operationTimes is a dynamically allocated array which stores parsed operation times from PBINT.txt
// Dynamic memory is used for this because we do not know how many operation times are defined in PBINT.txt during compilation
opTime* operationTimes = NULL;
// the number of elements in operationTimes array
int numOperationTimes = 0;

/* Initialization routine */
void setup() {
  // begin Serial, and ensure that our analogWrite() resolution is set to the maximum resolution (2^12 == 4096)
  Serial.begin(2000000);
  analogWriteResolution(12);
  // delay(4000); // this delay is not needed, however it provides some time to open Serial monitor for debugging

  Serial.println("\nInitializing");

  // configure pin modes
  configurePins();

  // turn on 3VR rail
  digitalWrite(HYPNOS_3VR, LOW);
  Serial.println("initializing SD");

  // Verify that SD can be initialized; stop the program if it can't.
  if (!BeginSD()) {
    Serial.println("SD failed to initialize.");
    initializationFailFlash();
  }
  SD.end();

  // Initialize I2C communication
  Wire.begin();

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("RTC failed to begin.");
    initializationFailFlash();
  }

  // Check if RTC lost power; adjust the clock to the compilation time if it did.
  // Ideally, the RTC will adjust the time based on the actual time of day, rather than the compilation time of the sketch
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, adjusting...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // printing the current RTC time for debugging purposes (doing this is not necassary)
  char date[10] = "hh:mm:ss";
  rtc.now().toString(date);
  Serial.print("Current RTC Time: ");
  Serial.println(date);

  // reset rtc alarms
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);

  // set RTC !INT/SQR pin to interrupt mode
  rtc.writeSqwPinMode(DS3231_OFF);

  // disable alarm 2 since we are not using it
  rtc.disableAlarm(2);

  // RTC initialization done, end wire and turn off 3v rail
  Wire.end();
  digitalWrite(HYPNOS_3VR, HIGH);

  // calculate sinc filter table for upsampling values stored in outputSampleBuffer
  calculateUpsampleSincFilterTable();
  
  // load operation times from PBINT.txt file stored on SD card, parses and stores operation times into 
  // dynamically allocated array: opTimes *operationTimes
  // returns False upon failure and initiates fail flash
  if (!LoadOperationTimes()) {
    Serial.println("Reading operation times from SD failed!");
    initializationFailFlash();
  }

  // load playback sound from SD card and stores values to outputSampleBuffer
  // returns False upon failure and initiates fail flash
  if (!LoadSound((char*)playback_filename)) {
    Serial.println("Reading playback sound from SD failed!");
    initializationFailFlash();
  }

  // enable and disable isr timer for initial setup of timer interrupt
  startISRTimer(outputSampleDelayTime, OutputUpsampledSample);
  stopISRTimer();

  // perform setup playback to confirm that Playback() is working (just a precaution, can be commented out)
  Serial.println("Performing setup playback");
  Playback();

  Serial.println("Setup complete");
  Serial.println();

  // setupOperation() returns true if the RTC time is within an operation interval,
  // otherwise, returns false. This is also where RTC alarm is set based on time.
  if (!setupOperation()) {
    goToSleep(sleepmode);
  }
}

/* Tasks to perform when device is awake */
void loop() {
  Playback();
  // may add option to add delay between playbacks in the future (will likely be set in PBINT.txt)
}

/* Put device to sleep in selected sleep mode */
void goToSleep(SLEEPMODES mode) {
  Serial.println("Going to sleep");

  // end USB Serial, probably can be commented out
  USBDevice.detach();
  USBDevice.end();

  // ensure that the correct sleep mode is set
  PM->SLEEPCFG.bit.SLEEPMODE = mode;
  while(PM->SLEEPCFG.bit.SLEEPMODE != mode);

  // go to sleep by disabling serial bus (__DSB) and calling wait for interrupt (__WFI)
  __DSB();
  __WFI();
}

/* Returns the current RTC time */
uint32_t getRTCTime() {
  uint32_t timeSeconds;

  digitalWrite(HYPNOS_3VR, LOW);
  Wire.begin();

  timeSeconds = rtc.now().secondstime();

  Wire.end();
  digitalWrite(HYPNOS_3VR, HIGH);

  return timeSeconds;
}

/* Set pin modes */
void configurePins() {
  pinMode(AUD_OUT, OUTPUT);
  pinMode(HYPNOS_5VR, OUTPUT);
  pinMode(HYPNOS_3VR, OUTPUT);
  pinMode(SD_CS, OUTPUT);  
  pinMode(AMP_SD, OUTPUT);
}

/* load operation times from "PBINT.txt" */
bool LoadOperationTimes() {

  char dir[17];
  strcpy(dir, "/PBINT/PBINT.txt");

  // begin communication with SD
  ResetSPI();
  digitalWrite(HYPNOS_3VR, LOW);

  if (!BeginSD()) {
    digitalWrite(HYPNOS_3VR, HIGH);
    return false;
  }

  if (!SD.exists(dir)) {
    digitalWrite(HYPNOS_3VR, HIGH);
    return false;
  }

  // open PBINT.txt for reading
  data = SD.open(dir, FILE_READ);

  if (!data) {
    digitalWrite(HYPNOS_3VR, HIGH);
    return false;
  }

  // read and store playback intervals
  while(data.available()) {
    //int minutes = data.readStringUNtil('\n').toInt();
    short s_hour = data.readStringUntil(':').toInt();
    short s_minute = data.readStringUntil('\n').toInt();
    addOperationTime(s_hour, s_minute);
  }

  data.close();
  SD.end();
  digitalWrite(HYPNOS_3VR, HIGH);

  // print playback intervals
  Serial.println("Printing operation hours");
  for (int i = 0; i < numOperationTimes; i = i + 2) {
    Serial.printf("%02d:%02d - %02d:%02d\n", operationTimes[i].hour, operationTimes[i].minute, operationTimes[i + 1].hour, operationTimes[i + 1].minute);
  }
  Serial.println();

  return true;
}

/* allocate memory and store operation time */
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


/* Set RTC alarm to the corresponding operation time. If RTC time is within an operation interval,
 RTC alarm is set to the end of this operation interval and function returns True. Otherwise,
 RTC alarm is set to the start of the next operation interval and function returns False. */
bool setupOperation() {
  // perform playback continously if DEFAULT_PLAYBACK_INT is 0
  if (DEFAULT_PLAYBACK_INT == 0) return 1;

  // get the current time from RTC
  digitalWrite(HYPNOS_3VR, LOW);
  Wire.begin();

  // Serial.printf("The current temperature is: %.1f\n", rtc.getTemperature());
  DateTime nowDT = rtc.now();

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

  // schedule RTC alarm totalMinutes into the future
  digitalWrite(HYPNOS_3VR, LOW);
  Wire.begin();

  if(!rtc.setAlarm1(
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

/* Loads playback sound file from the SD card into the audio output buffer as samples. 
 Once loaded, the data will remain in-place until LoadSound() is called again. This means
 that all future calls to Playback() will play back the same audio data. */
bool LoadSound(char* fname) {
  ResetSPI(); 
  digitalWrite(HYPNOS_3VR, LOW);

  char dir[28];

  strcpy(dir, "/PBAUD/");
  strcat(dir, fname);

  if (!BeginSD()) {
    //Serial.println("SD failed to initialize.");
    digitalWrite(HYPNOS_3VR, HIGH);
    return false;
  }

  //Serial.println("SD initialized successfully, checking directory...");

  if (!SD.exists(dir)) {
    //Serial.print("LoadSound failed: directory ");
    //Serial.print(dir);
    //Serial.println(" could not be found.");
    digitalWrite(HYPNOS_3VR, HIGH);
    return false;
  }

  //Serial.println("Directory found. Opening file...");

  data = SD.open(dir, FILE_READ);

  if (!data) {
    //Serial.print("LoadSound failed: file ");
    //Serial.print(dir);
    //Serial.println(" could not be opened.");
    digitalWrite(HYPNOS_3VR, HIGH);
    return false;
  }

  //Serial.println("Opened file. Loading samples...");

  int fsize = data.size();
  short buff;

  playbackSampleCount = fsize / 2;

  for (int i = 0; i < playbackSampleCount; i++) {
    if ((2 * i) < (fsize - 1)) {
      data.read((byte*)(&buff), 2);
      outputSampleBuffer[i] = buff;
    } else {
      break;
    }
  }

  data.close();
  SD.end();

  digitalWrite(HYPNOS_3VR, HIGH);

  return true;
}

/* Plays back the sound loaded on SD card using the vibration exciter */
void Playback() {
  stopISRTimer();

  //Serial.println("Enabling amplifier...");

  digitalWrite(HYPNOS_5VR, HIGH); // enable 5VR
  analogWrite(AUD_OUT, 2048);     // set AUD_OUT to 2048 (3.3V / 2)
  digitalWrite(AMP_SD, HIGH);     // enable amplifier
  delay(100);                     // short delay for analog low pass filter

  //Serial.println("Amplifier enabled. Beginning playback ISR...");

  startISRTimer(outputSampleDelayTime, OutputUpsampledSample);

  while (outputSampleBufferPtr < playbackSampleCount) {}

  stopISRTimer();

  //Serial.println("Finished playback ISR.\nShutting down amplifier...");

  outputSampleBufferPtr = 0;

  digitalWrite(AMP_SD, LOW);    // disable amplifier
  analogWrite(AUD_OUT, 0);      // set AUD_OUT to 0V
  digitalWrite(HYPNOS_5VR, LOW);  // disable 5VR

  //Serial.println("Amplifer shut down.");
}

/* Upsamples playback sound and writes upsampled sample to AUD_OUT */
void OutputUpsampledSample() {
  analogWrite(AUD_OUT, nextOutputSample);

  if (outputSampleBufferPtr == playbackSampleCount) return;
  
  // store last location of upsampling input buffer
  int upsampleInputPtrCpy = upsampleInputPtr;
  
  // store value of sample in upsampling input buffer, and pad with zeroes
  int sample = upsampleInputC++ == 0 ? outputSampleBuffer[outputSampleBufferPtr++] : 0;
  upsampleInput[upsampleInputPtr++] = sample;
  
  if (upsampleInputC == AUD_OUT_UPSAMPLE_RATIO) upsampleInputC = 0;
  if (upsampleInputPtr == sincTableSizeUp) upsampleInputPtr = 0;

  // calculate upsampled value
  float upsampledSample = 0.0;
  for (int i = 0; i < sincTableSizeUp; i++) {
    upsampledSample += upsampleInput[upsampleInputPtrCpy++] * sincFilterTableUpsample[i];
    if (upsampleInputPtrCpy == sincTableSizeUp) upsampleInputPtrCpy = 0;
  }

  nextOutputSample = max(0, min(4095, round(upsampledSample)));
}


/* Calculates sinc filter table for upsampling a signal by @ratio
 @nz is the number of zeroes to use for the table */
void calculateUpsampleSincFilterTable() {
  int ratio = AUD_OUT_UPSAMPLE_RATIO;
  int nz = AUD_OUT_UPSAMPLE_FILTER_SIZE;
  // Build sinc function table for upsampling by @upsample_ratio
  int n = sincTableSizeUp;

  float ns[n];
  float ns_step = float(nz * 2) / (n - 1);

  float t[n];
  float t_step = 1.0 / (n - 1);

  for (int i = 0; i < n; i++) {
    ns[i] = float(-1.0 * nz) + ns_step * i;
    t[i] = t_step * i;
  }

  for (int i = 0; i < n; i++) {
    sincFilterTableUpsample[i] = sin(PI * ns[i]) / (PI * ns[i]); 
  }
  sincFilterTableUpsample[int(round((n - 1) / 2.0))] = 1.0;
  for (int i = 0; i < n; i++) {
    sincFilterTableUpsample[i] = sincFilterTableUpsample[i] * 0.5 * (1.0 - cos(2.0 * PI * t[i]));
  }
}

/* Reset SPI pins, called prior to SPI.begin() as a precaution */
void ResetSPI() {
  pinMode(23, INPUT);
  pinMode(24, INPUT);
  pinMode(10, INPUT);

  delay(20);

  pinMode(23, OUTPUT);
  pinMode(24, OUTPUT);
  pinMode(10, OUTPUT);
}

/* Attempts to initialize SD card on Hypnos */
bool BeginSD() {
  for (int i = 0; i < SD_OPEN_ATTEMPT_COUNT; i++) {
    if (SD.begin(SD_CS)) return true;
    //Serial.print("SD failed to initialize in SaveDetection on try #");
    //Serial.println(i);
    delay(SD_OPEN_RETRY_DELAY_MS);
  }

  return false;
}

/* Flashes Hypnos 3VR LED to indicate initialization error */
void initializationFailFlash() {
  while(1) {
    digitalWrite(HYPNOS_3VR, LOW);
    delay(500);
    digitalWrite(HYPNOS_3VR, HIGH);
    delay(500);
  }
}
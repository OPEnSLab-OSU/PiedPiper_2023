#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include "RTClib.h"
#include "SAMDTimerInterrupt.h"

#define AUD_OUT_SAMPLE_FREQ 4096
#define AUD_OUT_UPSAMPLE_RATIO 8 // sinc filter upsample ratio, ideally should be a power of 2
#define AUD_OUT_UPSAMPLE_FILTER_SIZE 10 // // upsample sinc filter number of zero crossings, more crossings will produce a cleaner result but will also use more processor time 
#define AUD_OUT_TIME 8

#define SD_OPEN_ATTEMPT_COUNT 10
#define SD_OPEN_RETRY_DELAY_MS 100

#define AUD_OUT A0

#define SLEEP_INT 12

#define HYPNOS_5VR 6
#define HYPNOS_3VR 5

#define SD_CS 11 // Chip select for SD
#define AMP_SD 9

#define PLAYBACK_INT 2000 // Milliseconds between playback [900000]

#define BEGIN_LOG_WAIT_TIME 10000 //3600000

enum SLEEPMODES
{
  IDLE0 = 0x0,
  IDLE1 = 0x1,
  IDLE2 = 0x2,
  STANDBY = 0x4,  
  HIBERNATE = 0x5,
  BACKUP = 0x6,
  OFF = 0x7
};

uint8_t sleepmode = STANDBY;

// Audio output stuff
volatile short outputSampleBuffer[AUD_OUT_SAMPLE_FREQ * AUD_OUT_TIME];
volatile int outputSampleBufferPtr = 0;
const int outputSampleDelayTime = 1000000 / (AUD_OUT_SAMPLE_FREQ * AUD_OUT_UPSAMPLE_RATIO);
//const int outputSampleDelayTime = 1000000 / AUD_OUT_SAMPLE_FREQ;
volatile int playbackSampleCount = AUD_OUT_SAMPLE_FREQ * AUD_OUT_TIME;

volatile int nextOutputSample = 0;

// sinc filter for upsampling
const int sincTableSizeUp = (2 * AUD_OUT_UPSAMPLE_FILTER_SIZE + 1) * AUD_OUT_UPSAMPLE_RATIO - AUD_OUT_UPSAMPLE_RATIO + 1;
float sincFilterTableUpsample[sincTableSizeUp];

    // circular input buffer for upsampling
volatile short upsampleInput[sincTableSizeUp];
volatile int upsampleInputPtr = 0;
volatile int upsampleInputC = 0;

char playback_filename[9] = "BMSB.PAD";

File data;

RTC_DS3231 rtc;

SAMDTimer ITimer(TIMER_TC3);

// Functions to start and stop the ISR timer (these are passed to the main Pied Piper class)
void stopISRTimer()
{
  ITimer.detachInterrupt();
  ITimer.restartTimer();
}

void startISRTimer(const unsigned long interval_us, void(*fnPtr)())
{
  ITimer.attachInterruptInterval(interval_us, fnPtr);
}

struct opTime {
  short hour;
  short minute;
  int minutes;
};

opTime* operationTimes = NULL;
int numOperationTimes = 0;

volatile bool operationSet = 0;

void setup() {
  Serial.begin(2000000);
  //analogReadResolution(12);
  analogWriteResolution(12);
  delay(4000);

  Serial.println("\nInitializing");

  // configure pin modes
  configurePins();

  // turn on 3v rail
  digitalWrite(HYPNOS_3VR, LOW);

  // Verify that SD can be initialized; stop the program if it can't.
  if (!BeginSD()) {
    Serial.println("SD failed to initialize.");
    initializationFailFlash();
  }

  char playback_sound_dir[20];
  sprintf(playback_sound_dir, "/PBAUD/%s", playback_filename);
  Serial.printf("Playback filename: %s\n", playback_filename);
  // Verify that the SD card has all the required files and the correct directory structure.
  if (SD.exists(playback_sound_dir)) Serial.println("SD card has correct directory structure.");
  else Serial.println("WARNING: SD card does not have the correct directory structure.");

  SD.end();

  Wire.begin();

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("RTC failed to begin.");
    initializationFailFlash();
  }

  // Check if RTC lost power; adjust the clock to the compilation time if it did.
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, adjusting...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  char date[10] = "hh:mm:ss";
  rtc.now().toString(date);
  Serial.print("Current RTC Time: ");
  Serial.println(date);

  // reset rtc alarms

  rtc.clearAlarm(1);
  rtc.clearAlarm(2);

  rtc.writeSqwPinMode(DS3231_OFF);

  rtc.disableAlarm(2);

  // end wire, turn off 3v rail
  Wire.end();
  digitalWrite(HYPNOS_3VR, HIGH);

  // calculate sinc filter for upsampling
  calculateUpsampleSincFilterTable();
  
  // load operations
  if (!LoadOperationTimes()) {
    Serial.println("Reading operation times from SD failed");
    initializationFailFlash();
  }

  // load sound
  if (!LoadSound(playback_filename)) {
    Serial.println("Reading playback sound from SD failed!");
    initializationFailFlash();
  }

  // enable disable isr timer
  startISRTimer(outputSampleDelayTime, OutputUpsampledSample);
  stopISRTimer();

  Serial.println("Performing setup playback");
  Playback();

  PM->SLEEPCFG.bit.SLEEPMODE = sleepmode;
  while(PM->SLEEPCFG.bit.SLEEPMODE != sleepmode);

  // setup pin interrupt for RTC alarm
  attachWakeUpISR();
  detachWakeUpISR();

  Serial.println("Setup complete");
  Serial.println();

  delay(1000);
}

void loop() {
  
  // set alarm
  bool performPlayback = setupOperation();

  // keep performing playback until alarm is triggered
  while (performPlayback) {
    Playback();
    // check if alarm was triggered via operationSet flag
    if (!operationSet) {
      // set next alarm
      setupOperation();
      break;
    }
  }

  uint32_t nowTime = getRTCTime();

  goToSleep();

  // CODE STARTS HERE AFTER SLEEP!

  uint32_t sleepTime = getRTCTime() - nowTime;

  initUSBDevice();

  Serial.println();
  Serial.print("I slept for ");
  Serial.print(sleepTime);
  Serial.println(" seconds");
}

// attach RTC alarm pin
void attachWakeUpISR() {
  attachInterrupt(digitalPinToInterrupt(SLEEP_INT), wakeUpISR, CHANGE);
}

// detach RTC alarm pin
void detachWakeUpISR() {
  detachInterrupt(digitalPinToInterrupt(SLEEP_INT));
}

// RTC alarm interrupt function
void wakeUpISR() {
  operationSet = 0;
}

// general procedure for going to sleep
void goToSleep() {
  Serial.println("Going to sleep");

  USBDevice.detach();
  USBDevice.end();
  USBDevice.standby();

  PM->SLEEPCFG.bit.SLEEPMODE = sleepmode;
  while(PM->SLEEPCFG.bit.SLEEPMODE != sleepmode);

  // PM->STDBYCFG.bit.RAMCFG = 0x2;
  // while(PM->STDBYCFG.bit.RAMCFG != 0x2);

  // PM->STDBYCFG.bit.FASTWKUP = 0x0;
  // while(PM->STDBYCFG.bit.FASTWKUP != 0x0);

  // enable sleep
  __DSB();
  __WFI();
}

void initUSBDevice() {
  USBDevice.init();
  USBDevice.attach();
  delay(3000);
}

uint32_t getRTCTime() {
  uint32_t timeSeconds;

  digitalWrite(HYPNOS_3VR, LOW);
  Wire.begin();

  timeSeconds = rtc.now().secondstime();

  Wire.end();
  digitalWrite(HYPNOS_3VR, HIGH);

  return timeSeconds;
}

void configurePins() {
  pinMode(AUD_OUT, OUTPUT);
  pinMode(HYPNOS_5VR, OUTPUT);
  pinMode(HYPNOS_3VR, OUTPUT);
  pinMode(SD_CS, OUTPUT);  
  pinMode(AMP_SD, OUTPUT);
  pinMode(SLEEP_INT, INPUT_PULLUP);
}

// load operation times from "PBINT.txt"
bool LoadOperationTimes() {

  char dir[17];

  strcpy(dir, "/PBINT/PBINT.txt");

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

void addOperationTime(int hour, int minute) {
  numOperationTimes += 1;
  operationTimes = (opTime*)realloc(operationTimes, numOperationTimes * sizeof(opTime));
  operationTimes[numOperationTimes - 1].hour = hour;
  operationTimes[numOperationTimes - 1].minute = minute;
  operationTimes[numOperationTimes - 1].minutes = hour * 60 + minute;
}


// setup RTC alarm based on current time
bool setupOperation() {
  detachWakeUpISR();

  // get the current time from RTC
  digitalWrite(HYPNOS_3VR, LOW);
  Wire.begin();

  DateTime nowDT = rtc.now();

  rtc.clearAlarm(1);

  Wire.end();
  digitalWrite(HYPNOS_3VR, HIGH);

  // for now all operations are the same for this time of day
  int nowMinutes = nowDT.hour() * 60 + nowDT.minute();
  int nowSecond = nowDT.second();

  int nextAlarmTime = -1;

  bool performPlayback = 0;

  // Device will wake once every 24 hours if no operation times are defined
  if (numOperationTimes != 0) {
    // check if current time is within an operation time
    for (int i = 0; i < numOperationTimes - 1; i += 2) {
      //Serial.printf("Now: %d, Start: %d, End: %d\n", nowMinutes, operationTimes[i].minutes, operationTimes[i + 1].minutes);
      if (nowMinutes >= operationTimes[i].minutes && nowMinutes < operationTimes[i + 1].minutes) {
        Serial.println("Performing playback...");
        performPlayback = 1;
        nextAlarmTime = operationTimes[i + 1].minutes - nowMinutes;
        break;
      }
    }

    // if current time is not within operation time then set to next available operation time
    if (!performPlayback) {
      for (int i = 0; i < numOperationTimes - 1; i += 2) {
        if (operationTimes[i].minutes > nowMinutes) {
          nextAlarmTime = operationTimes[i].minutes - nowMinutes;
          break;
        }
      }
    }

    // if nextAlarmTime wasn't set, then set to first operation time
    if (nextAlarmTime == -1) {
      nextAlarmTime = 1440 - nowMinutes + operationTimes[0].minutes;
    }
  } else {
    nextAlarmTime = 1440;
  }

  if (nextAlarmTime < 60) Serial.printf("Next alarm will happen in %d minutes", nextAlarmTime);
  else  Serial.printf("Next alarm will happen in %d hours and %d minutes", nextAlarmTime / 60, nextAlarmTime % 60);
  Serial.println();

  // schedule an alarm totalMinutes in the future
  digitalWrite(HYPNOS_3VR, LOW);
  Wire.begin();

  if(!rtc.setAlarm1(
      nowDT + TimeSpan(nextAlarmTime * 60 - nowSecond),
      DS3231_A1_Hour // this mode triggers the alarm when the hours, minutes and seconds match
  )) {
    Serial.println("Error, alarm wasn't set!");
  }

  Wire.end();
  digitalWrite(HYPNOS_3VR, HIGH);

  // attach interrupt
  attachWakeUpISR();

  // set flag
  operationSet = 1;

  // return true if it is time for playback
  return performPlayback;
}

// Loads sound data from the SD card into the audio output buffer. Once loaded, the data will
// remain in-place until LoadSound is called again. This means that all future calls to 
// Playback will play back the same audio data.
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

// Plays back a female mating call using the vibration exciter
void Playback() {
  stopISRTimer();

  //Serial.println("Enabling amplifier...");

  digitalWrite(HYPNOS_5VR, HIGH);
  analogWrite(AUD_OUT, 2048);
  digitalWrite(AMP_SD, HIGH);
  delay(100);

  //Serial.println("Amplifier enabled. Beginning playback ISR...");

  startISRTimer(outputSampleDelayTime, OutputUpsampledSample);

  while (outputSampleBufferPtr < playbackSampleCount) {}

  stopISRTimer();

  //Serial.println("Finished playback ISR.\nShutting down amplifier...");

  outputSampleBufferPtr = 0;

  digitalWrite(AMP_SD, LOW);
  analogWrite(AUD_OUT, 0);
  digitalWrite(HYPNOS_5VR, LOW);


  //Serial.println("Amplifer shut down.");
}


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


// calculates sinc filter table for upsampling a signal by @ratio
// @nz is the number of zeroes to use for the table
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

void ResetSPI() {
  pinMode(23, INPUT);
  pinMode(24, INPUT);
  pinMode(10, INPUT);

  delay(20);

  pinMode(23, OUTPUT);
  pinMode(24, OUTPUT);
  pinMode(10, OUTPUT);
}

bool BeginSD() {
  for (int i = 0; i < SD_OPEN_ATTEMPT_COUNT; i++) {
    if (SD.begin(SD_CS)) return true;
    //Serial.print("SD failed to initialize in SaveDetection on try #");
    //Serial.println(i);
    delay(SD_OPEN_RETRY_DELAY_MS);
  }

  return false;
}

void initializationFailFlash() {
  while(1) {
    digitalWrite(HYPNOS_3VR, LOW);
    delay(500);
    digitalWrite(HYPNOS_3VR, HIGH);
    delay(500);
  }
}
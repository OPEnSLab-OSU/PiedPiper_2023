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

#define POWER_PIN 10

#define HYPNOS_5VR 6
#define HYPNOS_3VR 5

#define SD_CS 11 // Chip select for SD
#define AMP_SD 9

#define PLAYBACK_INT 2000 // Milliseconds between playback [900000]

#define BEGIN_LOG_WAIT_TIME 10000 //3600000

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

void setup() {

  Serial.begin(2000000);

  analogWriteResolution(12);

  delay(4000);

  Serial.println("Initializing");


  pinMode(AUD_OUT, OUTPUT);
  pinMode(HYPNOS_5VR, OUTPUT);
  pinMode(HYPNOS_3VR, OUTPUT);
  pinMode(SD_CS, OUTPUT);  
  pinMode(AMP_SD, OUTPUT);
  pinMode(SLEEP_INT, INPUT); // DO NOT PULL UP!!!!!!!!!!!!!!
  pinMode(POWER_PIN, OUTPUT);

  digitalWrite(POWER_PIN, LOW);

  Serial.println("Testing SD...");

  delay(1000);

  digitalWrite(HYPNOS_3VR, LOW);

  if (!SD.begin(SD_CS))
  {
    Serial.println("SD failed to initialize.");
  }

  // char playback_sound_dir[20];
  // sprintf(playback_sound_dir, "/PBAUD/%s", playback_filename);
  // Serial.printf("Playback filename: %s\n", playback_filename);
  // // Verify that the SD card has all the required files and the correct directory structure.
  // if (SD.exists(playback_sound_dir)) Serial.println("SD card has correct directory structure.");
  // else Serial.println("WARNING: SD card does not have the correct directory structure.");

  SD.end();

  Wire.begin();

  Serial.println("Initializing RTC...");

  delay(1000);

  // Initialize RTC
  if (!rtc.begin())
  {
    Serial.println("RTC failed to begin.");
  }

  // Check if RTC lost power; adjust the clock to the compilation time if it did.
  if (rtc.lostPower())
  {
    Serial.println("RTC lost power, adjusting...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  char date[10] = "hh:mm:ss";
  rtc.now().toString(date);
  Serial.println(date);

  rtc.clearAlarm(1);
  rtc.clearAlarm(2);

  rtc.writeSqwPinMode(DS3231_OFF);

  rtc.disableAlarm(2);  

  Wire.end();
  digitalWrite(HYPNOS_3VR, HIGH);

  Serial.println("Setup complete");
  Serial.println(); 

  delay(1000);
}

void loop() {

  delay(10000);

  digitalWrite(HYPNOS_3VR, LOW);

  Wire.begin();

  rtc.clearAlarm(1);

  Serial.println("Setting alarm 10 second alarm");

  if(!rtc.setAlarm1(
      rtc.now() + TimeSpan(15),
      DS3231_A1_Second // this mode triggers the alarm when the minutes match
  )) {
    Serial.println("Error, alarm wasn't set!");
  }

  // Serial.println("waiting for alarm");
  // while(!rtc.alarmFired(1))
  //   ;

  // Serial.println("Setting alarm to wake up in 30 seconds");
  
  // if(!rtc.setAlarm1(
  //     rtc.now() + TimeSpan(30),
  //     DS3231_A1_Second // this mode triggers the alarm when the minutes match
  // )) {
  //   Serial.println("Error, alarm wasn't set!");
  // }

  Wire.end();
  digitalWrite(HYPNOS_3VR, HIGH);

  Serial.println("shutting off");

  delay(1000);

  pinMode(POWER_PIN, INPUT);

  delay(1000);

}

void powerOn() {
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH);
}

void powerOff() {
  pinMode(POWER_PIN, INPUT);
}

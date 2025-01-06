/*
  A test of PiedPiperPlayback in the Pied Piper library. This code loads playback sound and operation times from SD
  card, then determines whether to sleep or perform playback based on operation times and RTC time.
*/

#include <PiedPiper.h>

char settingsFilename[] = "SETTINGS.txt";   // settings filename (loaded from SD card)

PiedPiperPlayback p = PiedPiperPlayback();  // Pied Piper Playback object

void setup() {
    Serial.begin(2000000);

    delay(2000);

    Serial.println("Ready");

    // do necassary initializations (ISR, sinc filter table, configure pins)
    p.init();

    // turn on HYPNOS 3V rail
    p.HYPNOS_3VR_ON();

    // check if SD can be initialized, if it can, load playback sound and operation times
    if (!p.SDCard.initialize()) {
      Serial.println("SD card cannot be initialized");
      p.initializationFail();
    } else {
        p.SDCard.begin();
        if (!p.loadSettings(settingsFilename)) Serial.println("loadSettings() error");
        if (!p.loadSound(p.playbackFilename)) Serial.println("loadSound() error");
        if (!p.loadOperationTimes(p.operationTimesFilename)) Serial.println("loadOperationTimes() error");
        p.SDCard.end();
      }


    // begin I2C
    Wire.begin();

    DateTime dt;
    bool performPlayback = 1;

    // check if RTC can be initialized, if it can, get the current time and use operation times loaded from SD card
    // to set next alarm and determine whether trap should be playing back
    if (!p.RTCWrap.initialize()) {
      Serial.println("RTC cannot be initialized");
      p.initializationFail();
    } else {
      dt = p.RTCWrap.getDateTime();
      // char date[] = "YYMMDD-hh:mm:ss";
      // Serial.println(dt.toString(date));
      int minutesTillNextAlarm = p.OperationMan.minutesTillNextOperationTime(dt, performPlayback);
      p.RTCWrap.setAlarmSeconds(minutesTillNextAlarm * 60);
    }

    // end I2C, power off HYPNOS 3V rail
    Wire.end();

    p.HYPNOS_3VR_OFF();

    p.initializationSuccess();
  
    // if current time is outside of operation interval, go to sleep
    if (!performPlayback) p.SleepControl.goToSleep(OFF);

    // otherwise playback sound until alarm resets MCU
    p.HYPNOS_5VR_ON();

    p.amp.powerOn();

}

void loop() {
  // perform playback indefinetely
  p.performPlayback();
}
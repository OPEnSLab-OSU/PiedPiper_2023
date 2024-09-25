#include "piedPiper.h"
#include "SAMDTimerInterrupt.h" // Version 1.10.1, pulled on August 18, 2023

// Timer object for managing ISR's for audio recording and playback
SAMDTimer ITimer0(TIMER_TC3);

// Functions to start and stop the ISR timer (these are passed to the main Pied Piper class)
void stopISRTimer()
{
  ITimer0.detachInterrupt();
  ITimer0.restartTimer();
}

void startISRTimer(const unsigned long interval_us, void(*fnPtr)())
{
  ITimer0.attachInterruptInterval(interval_us, fnPtr);
}

// Create the Pied Piper class, and initialize any necessary global variables
piedPiper p(startISRTimer, stopISRTimer);
unsigned long currentTime = 0;
unsigned long lastTime = 0;

unsigned long windowCount = 0;

void setup() {
  // Begin serial communication
  Serial.begin(2000000);
  delay(4000);

  // Initialize Pied Piper by loading necassary files from SD card (settings, playback sound, master template), 
  // setting up RTC and camera module.
  p.init();
}

void loop() {
  lastTime = currentTime;
  currentTime = millis();

  // If the time of the last loop is later than the time of the current loop, then a clock rollover has occured.
  // Deal with this by resetting the times of all the last intermittent operations (playback, photos, etc) to 0.
  if (currentTime < lastTime)
  {
    p.ResetOperationIntervals();
  }

  // Check if the input sample buffer is full
  if (p.InputSampleBufferFull())
  {
    // Serial.println("buffer filled");
    // Move the data out of the buffer, reset the volatile sample pointer, and process the audio samples.
    p.ProcessData();

    // Check if the newly recorded audio contains a mating call
    // if (p.InsectDetection())

    if (windowCount < TEMPLATE_LENGTH) 
    {
      windowCount += 1;
    } 
    else 
    {
      // Use cross correlation with master template to check if newly recorded audio contains a mating call
      float correlationCoeff = p.CrossCorrelation();
      //Serial.println(millis() - currentTime);
      Serial.println(correlationCoeff);
      if (correlationCoeff >= CORRELATION_THRESH)
      {
        Serial.print("detection occured: ");
        Serial.println(correlationCoeff);
        // Continue recording audio for SAVE_DETECTION_DELAY_TIME milliseconds before taking photos and performing playback,
        // to make sure that all (or at least most) of the mating call is captured.
        while (millis() - currentTime < SAVE_DETECTION_DELAY_TIME)
        {
          if (p.InputSampleBufferFull())
          {
            p.ProcessData();
          }
        }
        p.StopAudio();
        // Save the recorded detection to the SD card, play back an artificial mating call, and take a photo when a detection occurs.
        p.SaveDetection();
        p.Playback();
        p.TakePhoto(p.GetDetectionNum());
        // writing all this data takes some time so our frequency buffers are extremely out of sync in the time domain
        // to solve this, reset frequency buffers and continue sampling audio.
        p.ResetFrequencyBuffers();
        windowCount = 0;
        p.StartAudioInput();
      }
    }
  }
  else
  {
    //Intermittent playback
    if (currentTime - p.GetLastPlaybackTime() > PLAYBACK_INT)
    {
      p.StopAudio();
      p.Playback();
      p.ResetFrequencyBuffers();
      windowCount = 0;
      p.StartAudioInput();
    }

    // If a detection has recently occured, then take images at a rapid pace (with interval IMG_INT milliseconds)
    if ((currentTime - p.GetLastDetectionTime() < IMG_TIME) && (currentTime - p.GetLastPhotoTime() > IMG_INT))
    {
      p.StopAudio();
      p.TakePhoto(p.GetDetectionNum());
      p.ResetFrequencyBuffers();
      windowCount = 0;
      p.StartAudioInput();
    }

    // If there have been no recent detections, then take photos with interval CTRL_IMG_INT
    else if (currentTime - p.GetLastPhotoTime() > CTRL_IMG_INT)
    {
      p.StopAudio();
      p.TakePhoto(0);
      p.ResetFrequencyBuffers();
      windowCount = 0;
      p.StartAudioInput();
    }

    // Occasionally log the current millisecond timer value to a text file to show that the trap is still alive.
    if (currentTime - p.GetLastLogTime() > LOG_INT)
    {
      p.StopAudio();
      p.LogAliveRTC();
      p.ResetFrequencyBuffers();
      windowCount = 0;
      p.StartAudioInput();
      //p.LogAlive();
    }
  }
}

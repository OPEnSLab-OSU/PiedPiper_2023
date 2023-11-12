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

#define HYPNOS_5VR 6
#define HYPNOS_3VR 5

#define SD_CS 11 // Chip select for SD
#define AMP_SD 9

#define PLAYBACK_INT 2000 // Milliseconds between playback [900000]

#define BEGIN_LOG_WAIT_TIME 10000 //3600000

// Audio output stuff
volatile static short outputSampleBuffer[AUD_OUT_SAMPLE_FREQ * AUD_OUT_TIME];
volatile static int outputSampleBufferPtr = 0;
static const int outputSampleDelayTime = 1000000 / (AUD_OUT_SAMPLE_FREQ * AUD_OUT_UPSAMPLE_RATIO);
//static const int outputSampleDelayTime = 1000000 / AUD_OUT_SAMPLE_FREQ;
static volatile int playbackSampleCount = AUD_OUT_SAMPLE_FREQ * AUD_OUT_TIME;

static volatile int nextOutputSample = 0;

// sinc filter for upsampling
static const int sincTableSizeUp = (2 * AUD_OUT_UPSAMPLE_FILTER_SIZE + 1) * AUD_OUT_UPSAMPLE_RATIO - AUD_OUT_UPSAMPLE_RATIO + 1;
static float sincFilterTableUpsample[sincTableSizeUp];

    // circular input buffer for upsampling
static volatile short upsampleInput[sincTableSizeUp];
static volatile int upsampleInputPtr = 0;
static volatile int upsampleInputC = 0;

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


void setup() {
  Serial.begin(2000000);
  //analogReadResolution(12);
  analogWriteResolution(12);
  delay(4000);

  Serial.println("Initializing");

  pinMode(AUD_OUT, OUTPUT);
  pinMode(HYPNOS_5VR, OUTPUT);
  pinMode(HYPNOS_3VR, OUTPUT);
  pinMode(SD_CS, OUTPUT);  
  pinMode(AMP_SD, OUTPUT);

  digitalWrite(HYPNOS_3VR, LOW);

  Serial.println("Testing SD...");

  delay(1000);

  // Verify that SD can be initialized; stop the program if it can't.
  if (!SD.begin(SD_CS))
  {
    Serial.println("SD failed to initialize.");
  }

  char playback_sound_dir[20];
  sprintf(playback_sound_dir, "/PBAUD/%s", playback_filename);
  Serial.printf("Playback filename: %s\n", playback_filename);
  // Verify that the SD card has all the required files and the correct directory structure.
  if (SD.exists(playback_sound_dir)) Serial.println("SD card has correct directory structure.");
  else Serial.println("WARNING: SD card does not have the correct directory structure.");

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

  // Disable the 3-volt rail.
  digitalWrite(HYPNOS_3VR, HIGH);

  Wire.end();

  calculateUpsampleSincFilterTable();

  ITimer.attachInterruptInterval(1000000, NULL);

  LoadSound(playback_filename);
  Playback();

  delay(10000);
}

void loop() {
  Playback();
}


// Loads sound data from the SD card into the audio output buffer. Once loaded, the data will
// remain in-place until LoadSound is called again. This means that all future calls to 
// Playback will play back the same audio data.
bool LoadSound(char* fname) {
  //stopISRTimer();
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


void OutputUpsampledSample(void) {
  analogWrite(AUD_OUT, nextOutputSample);

  if (outputSampleBufferPtr == playbackSampleCount) { return; }
  
  // store last location of upsampling input buffer
  int upsampleInputPtrCpy = upsampleInputPtr;
  
  // store value of sample in upsampling input buffer, and pad with zeroes
  if (upsampleInputC++ == 0) {
    upsampleInput[upsampleInputPtr++] = outputSampleBuffer[outputSampleBufferPtr++];
  } else {
    upsampleInput[upsampleInputPtr++] = 0;
  }
  if (upsampleInputC == AUD_OUT_UPSAMPLE_RATIO) { upsampleInputC = 0; }
  if (upsampleInputPtr == sincTableSizeUp) { upsampleInputPtr = 0; }

  // calculate upsampled value
  float upsampledSample = 0.0;
  for (int i = 0; i < sincTableSizeUp; i++) {
    upsampledSample += upsampleInput[upsampleInputPtrCpy++] * sincFilterTableUpsample[i];
    if (upsampleInputPtrCpy == sincTableSizeUp) { upsampleInputPtrCpy = 0; }
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

void ResetSPI()
{
  pinMode(23, INPUT);
  pinMode(24, INPUT);
  pinMode(10, INPUT);

  delay(20);

  pinMode(23, OUTPUT);
  pinMode(24, OUTPUT);
  pinMode(10, OUTPUT);
}

bool BeginSD()
{
  for (int i = 0; i < SD_OPEN_ATTEMPT_COUNT; i++)
  {
    if (SD.begin(SD_CS))
    {
      return true;
    }
    //Serial.print("SD failed to initialize in SaveDetection on try #");
    //Serial.println(i);
    delay(SD_OPEN_RETRY_DELAY_MS);
  }

  return false;
}

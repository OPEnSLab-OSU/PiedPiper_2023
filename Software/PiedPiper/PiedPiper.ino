/*
  This code is for the full featured Pied Piper
*/

#include <PiedPiper.h>

// detection algorithm settings
#define CORRELATION_THRESH 0.75
#define NOISE_REMOVAL_SIZE 8
#define NOISE_REMOVAL_THRESH 3.5 
#define TIME_AVERAGING 4    // time domain smoothing size / length of raw frequency buffer
#define FREQ_SMOOTHING 4    // frequency domain smoothing size

// preamp calibration values (audio input circuit gain is adjusted so that the maximum value read from ADC is in this range:
// [PREAMP_CALIBRATION - PREAMP_CALIBRATION_THESH, PREAMP_CALIBRATION + PREAMP_CALIBRATION_THESH]
#define PREAMP_CALIBRATION 3500 
#define PREAMP_CALIBRATION_THRESH 50

#define TEMPLATE_LENGTH 13  // length of correlation template (in windows)

#define REC_TIME 8  // length of processed frequency buffer (in seconds)

const uint16_t FREQ_WIN_COUNT = REC_TIME * FFT_SAMPLE_RATE / FFT_WINDOW_SIZE; // number of windows for processed frequency buffer data

char settingsFilename[] = "SETTINGS.txt"; // settings filename (loaded from SD card)

uint16_t correlationTemplate[TEMPLATE_LENGTH][FFT_WINDOW_SIZE_BY2]; // buffer for template data
uint16_t processedFreqs[FREQ_WIN_COUNT][FFT_WINDOW_SIZE_BY2];       // buffer for processed frequency data
uint16_t rawFreqs[TIME_AVERAGING][FFT_WINDOW_SIZE_BY2];             // buffer for raw frequency data

// buffers for FFT
float vReal[FFT_WINDOW_SIZE];
float vImag[FFT_WINDOW_SIZE];

// scratch pad arrays
uint16_t scratch[FFT_WINDOW_SIZE_BY2];
uint16_t scratch2[FFT_WINDOW_SIZE_BY2];

PiedPiperMonitor p = PiedPiperMonitor(); // Pied Piper Monitor object (includes camera, digital pot, temperature sensor)

CircularBuffer<uint16_t> rawFreqsBuffer = CircularBuffer<uint16_t>();       // circular buffer for raw frequency buffer
CircularBuffer<uint16_t> processedFreqsBuffer = CircularBuffer<uint16_t>(); // circular buffer for processed frequency buffer

CrossCorrelation correlation = CrossCorrelation(FFT_SAMPLE_RATE, FFT_WINDOW_SIZE);

void setup() {
  Serial.begin(115200);

    delay(2000);

    Serial.println("Ready");

    // do necassary initializations (ISR, sinc filter table, configure pins)
    p.init();

    // turn on HYPNOS 3V rail
    p.Hypnos_3VR_ON();

    // check if SD can be initialized, if it can, load settings, calibration sound, correlation template and operation times
    if (!p.SDCard.initialize()) Serial.println("SD card cannot be initialized");
    else {
      p.SDCard.begin();
  
      if (!p.loadSettings(settingsFilename)) Serial.println("loadSettings() error");
      if (!p.loadSound(p.calibrationFilename)) Serial.println("loadSound() error");
      if (!p.loadTemplate(p.templateFilename, (uint16_t *)correlationTemplate, TEMPLATE_LENGTH)) Serial.println("loadTemplate() error");
      if (!p.loadOperationTimes(p.operationTimesFilename)) Serial.println("loadOperationTimes() error");
    }
    p.SDCard.end();


    // begin I2C
    Wire.begin();

    // check if RTC can be initialized, if it can, get the current time and use operation times loaded from SD card
    // to set next alarm and determine whether trap should be playing back
    DateTime dt;
    bool trapActive = 1;
    
    if (!p.RTCWrap.initialize()) Serial.println("RTC cannot be initialized");
    else {
      dt = p.RTCWrap.getDateTime();
      int minutesTillNextAlarm = p.OperationMan.minutesTillNextOperationTime(dt, trapActive);
      p.RTCWrap.setAlarmSeconds(minutesTillNextAlarm * 60);
    }

    // check if digital pot and temp sensor can be initialized
    p.Hypnos_5VR_ON();

    if (!p.preAmp.initialize()) Serial.println("Digital pot cannot be initialized");
    if (!p.tempSensor.begin()) Serial.println("Temp sensor cannot be initialized");

    // end I2C
    Wire.end();

    // check if camera can be initialized
    if (!p.camera.initialize()) Serial.println("TTL Camera cannot be initialized");

    p.initializationSuccess();
  
    // if current time is outside of operation interval, go to sleep
    if (!trapActive) p.SleepControl.goToSleep(OFF);

    // set circular buffers and template
    rawFreqsBuffer.setBuffer((uint16_t *)rawFreqs, FFT_WINDOW_SIZE_BY2, TIME_AVERAGING);
    processedFreqsBuffer.setBuffer((uint16_t *)processedFreqs, FFT_WINDOW_SIZE_BY2, FREQ_WIN_COUNT);
    correlation.setTemplate((uint16_t *)correlationTemplate, FFT_WINDOW_SIZE_BY2, TEMPLATE_LENGTH, 50, 100);

    // perform calibration (set digital pot)
    p.amp.powerOn();
    p.calibrate(3500, 50);
    p.amp.powerOff();

    p.Hypnos_5VR_OFF();

    // load playback sound
    p.SDCard.begin();
    p.loadSound(p.playbackFilename);
    p.SDCard.end();

    p.Hypnos_3VR_OFF();

    // begin audio sampling
    p.startAudioInput();

}

float correlationThresh = 0.0;

void loop() {
  // check if audio input buffer is filled
  if (!p.audioInputBufferFull(vReal)) return;

  // prepare arrays for FFT
  for (int i = 0; i < FFT_WINDOW_SIZE; i++) {
    vImag[i] = 0;
  }

  FFT(vReal, vImag, FFT_WINDOW_SIZE);

  // stochastic noise removal
  AlphaTrimming<float>(vReal, vImag, FFT_WINDOW_SIZE_BY2, NOISE_REMOVAL_SIZE, NOISE_REMOVAL_THRESH);

  // copy results to temporary buffer
  for (int i = 0; i < FFT_WINDOW_SIZE_BY2; i++) {
    scratch[i] = uint16_t(round(vImag[i]));
  }

  // store 'raw' data in buffer for time smoothing
  rawFreqsBuffer.pushData(scratch);

  // time smoothing on data
  TimeSmoothing<uint16_t>((uint16_t *)rawFreqs, scratch, FFT_WINDOW_SIZE_BY2, TIME_AVERAGING);

  // smoothing frequency domain of time smoothed data
  FrequencySmoothing<uint16_t>(scratch, scratch2, FFT_WINDOW_SIZE, FREQ_SMOOTHING);

  // store time/frequency smoothed data to processed data buffer
  processedFreqsBuffer.pushData(scratch2);

  // correlation with processed data and template
  correlationThresh = correlation.correlate((uint16_t *)processedFreqs, processedFreqsBuffer.getCurrentIndex(), FREQ_WIN_COUNT);

  // WIP, do stuff if correlation is positive...
  if (correlationThresh >= CORRELATION_THRESH) {
    Serial.println("positive correlation");
  }
}

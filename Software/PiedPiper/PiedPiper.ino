/*
  This code is for the full featured Pied Piper
*/

#include <PiedPiper.h>

char settingsFilename[] = "SETTINGS.txt";   // settings filename (loaded from SD card)

// detection algorithm settings
#define CORRELATION_THRESH 0.90           // positive correlation threshold
#define CORRELATION_COUNT 8               // number of positive correlations to be considered a detection
#define CORRELATION_MAX_INTERVAL 2000000  // maximum time between positive correlations (in microseconds) before correlation count is reset
#define NOISE_REMOVAL_SIZE 8              // number of adjacent samples to use for computing sample deviation
#define NOISE_REMOVAL_THRESH 1.0          // minumum sample deviation (samples below this value are considered noise)
#define TIME_SMOOTHING 2                  // time domain smoothing size of raw frequency buffer
#define FREQ_SMOOTHING 2                  // frequency domain smoothing size

// preamp calibration values (audio input circuit gain is adjusted so that the maximum value read from ADC is in this range:
// [PREAMP_CALIBRATION - PREAMP_CALIBRATION_THESH, PREAMP_CALIBRATION + PREAMP_CALIBRATION_THESH]
#define PREAMP_CALIBRATION 2500 
#define PREAMP_CALIBRATION_THRESH 100

#define TEMPLATE_LENGTH 13  // length of correlation template (in windows)

#define REC_TIME 8  // length of processed frequency buffer (in seconds)

const uint16_t FREQ_WIN_COUNT = REC_TIME * FFT_SAMPLE_RATE / FFT_WINDOW_SIZE; // number of windows for processed frequency buffer data

uint16_t rawSamples[FFT_WINDOW_SIZE][FREQ_WIN_COUNT];
uint16_t correlationTemplate[FFT_WINDOW_SIZE_BY2][TEMPLATE_LENGTH]; // buffer for template data
uint16_t processedFreqs[FFT_WINDOW_SIZE_BY2][FREQ_WIN_COUNT];       // buffer for processed frequency data
uint16_t rawFreqs[FFT_WINDOW_SIZE_BY2][TIME_SMOOTHING];             // buffer for raw frequency data

// buffers for newly recorded samples and FFT
float vReal[FFT_WINDOW_SIZE];
float vImag[FFT_WINDOW_SIZE];

// scratch pad arrays
uint16_t samples[FFT_WINDOW_SIZE];
uint16_t scratch[FFT_WINDOW_SIZE_BY2];

PiedPiperMonitor p = PiedPiperMonitor(); // Pied Piper Monitor object (includes camera, digital pot, temperature sensor)

CircularBuffer<uint16_t> rawSamplesBuffer = CircularBuffer<uint16_t>();     // circular buffer for raw samples
CircularBuffer<uint16_t> rawFreqsBuffer = CircularBuffer<uint16_t>();       // circular buffer for raw frequency data
CircularBuffer<uint16_t> processedFreqsBuffer = CircularBuffer<uint16_t>(); // circular buffer for processed frequency data

CrossCorrelation correlation = CrossCorrelation(FFT_SAMPLE_RATE, FFT_WINDOW_SIZE);

float correlationCoefficient = 0.0;
float averagedCorrelationCoefficient[CORRELATION_COUNT];

uint16_t correlationCount = 0;
uint32_t lastCorrelationTime = 0;

void setup() {
  Serial.begin(2000000);

  delay(2000);

  Serial.println("Ready");

  // do necassary initializations (ISR, sinc filter table, configure pins)
  p.init();

  // turn on HYPNOS 3V rail
  p.Hypnos_3VR_ON();

  // check if SD can be initialized, if it can, load settings, calibration sound, correlation template and operation times
  if (!p.SDCard.begin()) Serial.println("SD card cannot be initialized");
  else {
    if (!p.loadSettings(settingsFilename)) Serial.println("loadSettings() error");
    if (!p.loadTemplate(p.templateFilename, (uint16_t *)correlationTemplate, TEMPLATE_LENGTH)) Serial.println("loadTemplate() error");
    if (!p.loadOperationTimes(p.operationTimesFilename)) Serial.println("loadOperationTimes() error");
  }


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
    //p.RTCWrap.setAlarmSeconds(minutesTillNextAlarm * 60);
  }

  // check if digital pot and temp sensor can be initialized
  p.Hypnos_5VR_ON();  

  if (!p.preAmp.initialize()) Serial.println("Digital pot cannot be initialized");
  if (!p.tempSensor.begin()) Serial.println("Temp sensor cannot be initialized");

  // check if camera can be initialized
  if (!p.camera.initialize()) Serial.println("TTL Camera cannot be initialized");

  p.initializationSuccess();

  // if current time is outside of operation interval, go to sleep
  // if (!trapActive) p.SleepControl.goToSleep(OFF);

  // set circular buffers and template
  rawSamplesBuffer.setBuffer((uint16_t *)rawSamples, FFT_WINDOW_SIZE, FREQ_WIN_COUNT);
  rawFreqsBuffer.setBuffer((uint16_t *)rawFreqs, FFT_WINDOW_SIZE_BY2, TIME_SMOOTHING);
  processedFreqsBuffer.setBuffer((uint16_t *)processedFreqs, FFT_WINDOW_SIZE_BY2, FREQ_WIN_COUNT);
  correlation.setTemplate((uint16_t *)correlationTemplate, FFT_WINDOW_SIZE_BY2, TEMPLATE_LENGTH, 50, 90);

  // perform calibration (set pre-amp gain) 
  p.amp.powerOn();
  // p.frequencyResponse();
  if (!p.loadSound(p.calibrationFilename)) Serial.println("loadSound() error");

  //p.calibrate(PREAMP_CALIBRATION, PREAMP_CALIBRATION_THRESH);

  p.amp.powerOff();
  p.Hypnos_5VR_OFF();

  // load playback sound
  p.loadSound(p.playbackFilename);

  // end SD
  p.SDCard.end();

  // end I2C
  Wire.end();

  p.Hypnos_3VR_OFF();

  // begin audio sampling
  p.startAudioInput();

}

void loop() {
  // check if audio input buffer is filled (store samples to samples buffer)
  if (!p.audioInputBufferFull(samples)) return;

  // store raw samples in buffer
  rawSamplesBuffer.pushData(samples);

  // prepare arrays for FFT (copy samples to vReal and zero out vImag)
  for (int i = 0; i < FFT_WINDOW_SIZE; i++) {
    vReal[i] = samples[i];
    vImag[i] = 0;
  }

  FFT(vReal, vImag, FFT_WINDOW_SIZE);

  // stochastic noise removal
  AlphaTrimming<float>(vReal, vImag, FFT_WINDOW_SIZE, NOISE_REMOVAL_SIZE, NOISE_REMOVAL_THRESH);

  // copy results to temporary buffer
  for (int i = 0; i < FFT_WINDOW_SIZE_BY2; i++) {
    scratch[i] = uint16_t(round(vImag[i]));
  }

  // store 'raw' data in buffer for time smoothing
  rawFreqsBuffer.pushData(scratch);

  // time smoothing on data
  TimeSmoothing<uint16_t>((uint16_t *)rawFreqs, scratch, FFT_WINDOW_SIZE, TIME_SMOOTHING);

  // smoothing frequency domain of time smoothed data
  FrequencySmoothing<uint16_t>(scratch, samples, FFT_WINDOW_SIZE, FREQ_SMOOTHING);

  // store time/frequency smoothed data to processed data buffer
  for (int i = 0; i < FFT_WINDOW_SIZE_BY2; i++) {
    samples[i] = uint16_t(round(samples[i] * FREQ_WIDTH));
  }
  processedFreqsBuffer.pushData(samples);

  // correlation with processed data and template
  correlationCoefficient = correlation.correlate((uint16_t *)processedFreqs, processedFreqsBuffer.getCurrentIndex(), FREQ_WIN_COUNT);

  // WIP, do stuff if correlation is positive...
  if (correlationCoefficient >= CORRELATION_THRESH) {
    // reset correlation count if positive correlatioon didn't occur within CORRELATION_MAX_INTERVAL
    if (micros() - lastCorrelationTime > CORRELATION_MAX_INTERVAL) correlationCount = 0;
    averagedCorrelationCoefficient[correlationCount] = correlationCoefficient;
    correlationCount += 1;
    lastCorrelationTime = micros();
  }
  
  if (correlationCount == CORRELATION_COUNT) {
    // stop audio sampling
    p.stopAudio();

    Serial.println("Positive correlation");

    correlationCount = 0;

    // write detection data (structure: YYMMDD/hhmmss/)
    p.Hypnos_3VR_ON();
    Wire.begin();
    char date[] = "YYYYMMDD-hh:mm:ss";
    char buf[64] = { 0 };
    char buf2[64] = { 0 };
    p.RTCWrap.getDateTime().toString(date);
    Serial.println(date);
    Wire.end();

    strcat(buf, "/DATA/");

    p.SDCard.begin();
    // create DATA directory if it does not exist
    if (!SD.exists(buf)) SD.mkdir(buf);
    
    // creating directory with date YYMMDD
    strncat(buf + 6, date, 8);

    SD.mkdir(buf);

    // creating another directory with time hhmmss
    strcat(buf, "/");
    strncat(buf, date + 9, 2);
    strncat(buf, date + 12, 2);
    strncat(buf, date + 15, 2);
    SD.mkdir(buf);

    // storing directory in temp buffer
    strcpy(buf2, buf);
    // file for processed frequencies buffer
    strcat(buf, "/PFD.TXT");

    // write processed frequencies buffer to PFD.txt
    if (!p.SDCard.openFile(buf, FILE_WRITE)) Serial.println("openFile() error");
    else {
      p.writeCircularBufferToFile(&processedFreqsBuffer);
      p.SDCard.closeFile();
    }

    // file for raw samples
    strcpy(buf, buf2);
    strcat(buf, "/RAW.TXT");

    // Serial.println(buf);

    // write raw samples buffer to RAW.txt
    if (!p.SDCard.openFile(buf, FILE_WRITE)) Serial.println("openFile() error");
    else {
      p.writeCircularBufferToFile(&rawSamplesBuffer);
      p.SDCard.closeFile();
    }

    // store details of detection to DETS.txt
    strcpy(buf, buf2);
    strcat(buf, "/DETS.TXT");
    if (!p.SDCard.openFile(buf, FILE_WRITE)) Serial.println("openFile() error");
    else {
      correlationCoefficient = 0;
      for (int i = 0; i < CORRELATION_COUNT; i++) {
        correlationCoefficient += averagedCorrelationCoefficient[i];
      }
      correlationCoefficient /= CORRELATION_COUNT;
      p.SDCard.data.print(date);
      p.SDCard.data.print(" ");
      p.SDCard.data.print(correlationCoefficient, 3);
      // TODO: add temperature/humidity data here... (consider reading temp sensor data earlier, after reading time from RTC)

      // storing detection algorithm settings on next line
      p.SDCard.data.println();
      p.SDCard.data.print(FFT_SAMPLE_RATE);
      p.SDCard.data.print(" ");
      p.SDCard.data.print(FFT_WINDOW_SIZE);
      p.SDCard.data.print(" ");
      p.SDCard.data.print(NOISE_REMOVAL_SIZE);
      p.SDCard.data.print(" ");
      p.SDCard.data.print(NOISE_REMOVAL_THRESH);
      p.SDCard.data.print(" ");
      p.SDCard.data.print(TIME_SMOOTHING);
      p.SDCard.data.print(" ");
      p.SDCard.data.println(FREQ_SMOOTHING);
      p.SDCard.closeFile();
    }

    Serial.println(correlationCoefficient);
    
    // TODO: open file for storing photo, call camera.takePhoto(&p.SDCard.data) after opening file...
    // Hints: 
    //    - use buf2 to form path to photo file...
    //    - make sure to turn on Hypnos 5VR before taking photo


    p.SDCard.end();
    p.Hypnos_3VR_OFF();

    // reset buffers
    rawSamplesBuffer.clearBuffer();
    rawFreqsBuffer.clearBuffer();
    processedFreqsBuffer.clearBuffer();

    // perform playback...
    // p.amp.powerOn();
    // p.Hypnos_5VR_ON();
    // p.performPlayback();
    // p.amp.powerOff();
    // p.Hypnos_5VR_OFF();

    // restart audio sampling
    p.startAudioInput();
  }
}
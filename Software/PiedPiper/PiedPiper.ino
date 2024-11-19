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
#define NOISE_REMOVAL_THRESH 1.0          // minumum sample deviation (samples with deviation below this value are considered noise)
#define TIME_SMOOTHING 4                  // time domain smoothing size
#define FREQ_SMOOTHING 2                  // frequency domain smoothing size

// preamp calibration values (audio input circuit gain is adjusted so that the maximum value read from ADC is in this range:
// [PREAMP_CALIBRATION - PREAMP_CALIBRATION_THESH, PREAMP_CALIBRATION + PREAMP_CALIBRATION_THESH]
#define PREAMP_CALIBRATION 2500 
#define PREAMP_CALIBRATION_THRESH 100

#define TEMPLATE_LENGTH 13  // length of correlation template (in windows)

#define REC_TIME 8  // length of processed frequency buffer (in seconds)

#define DETECTION_PLAYBACK_DURATION 60000000 // mating call playback duration after a positive detection has occured (microseconds)

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
float averagedCorrelationCoefficient[CORRELATION_COUNT];  // for computing average correlation coefficient of consecutive detections

uint16_t correlationCount = 0; // see CORRELATION_COUNT and CORRELATION_MAX_INTERVAL
uint32_t lastCorrelationTime = 0xFFFFFFFF;

uint32_t microsTime = 0xFFFFFFFF;      // stores micros() each time audio input buffer fills
uint32_t prevMicrosTime = 0xFFFFFFFF;

uint32_t lastDetectionTime = 0xFFFFFFFF;

const char dateFormat[] = "YYYYMMDD-hh:mm:ss";
char date[] = "YYYYMMDD-hh:mm:ss";

DateTime dt;

float temperatureC = 0; // temperature in celsius
float humidity = 0; // humidity 

enum ERROR
{
  ERR_RTC = 0x1,
  ERR_PREAMP = 0x2,
  ERR_TEMPSEN = 0x4,
  ERR_CAMERA = 0x8,
  ERR_SETTING = 0x10,
  ERR_SOUND = 0x20,
  ERR_OPTIME = 0x40,
  ERR_TEMPLATE = 0x80
};

uint8_t err = 0;  // error code

void setup() {
  Serial.begin(2000000);

  delay(2000);

  Serial.println("Ready");

  // do necassary initializations (ISR, sinc filter table, configure pins)
  p.init();

  // turn on HYPNOS 3V rail
  p.Hypnos_3VR_ON();

  // check if RTC can be initialized, if it can, get the current time and use operation times loaded from SD card
  // to set next alarm and determine whether trap should be playing back
  Wire.begin();

  uint32_t minutesTillNextAlarm = 0;
  bool trapActive = 1;
  
  if (!p.RTCWrap.initialize()) {
    Serial.println("RTC cannot be initialized");
    err |= ERR_RTC;
  }
  else {
    dt = p.RTCWrap.getDateTime();
  }

  // check if SD can be initialized, if it can, load settings, calibration sound, correlation template and operation times
  if (!p.SDCard.begin()) {
    Serial.println("SD card cannot be initialized");
    p.initializationFail();
  } else {
    if (!p.loadSettings(settingsFilename)) {
      Serial.printf("loadSettings() error: %s\n", settingsFilename);
      err |= ERR_SETTING;
    }

    if (!p.loadTemplate(p.templateFilename, (uint16_t *)correlationTemplate, TEMPLATE_LENGTH)) {
      Serial.println("loadTemplate() error");
      err |= ERR_TEMPLATE;
    }

    if (!p.loadOperationTimes(p.operationTimesFilename)) {
      Serial.println("loadOperationTimes() error");
      err |= ERR_OPTIME;
    }

    if ((err & ERR_RTC) > 0) {
      minutesTillNextAlarm = p.OperationMan.minutesTillNextOperationTime(dt, trapActive);
      if (!p.RTCWrap.setAlarmSeconds(minutesTillNextAlarm * 60)) err |= ERR_RTC;
    }
  }

  // check if digital pot and temp sensor can be initialized
  p.Hypnos_5VR_ON();  

  if (!p.preAmp.initialize()) {
    Serial.println("Digital pot cannot be initialized");
    err |= ERR_PREAMP;
  }
  if (!p.tempSensor.begin()) {
    Serial.println("Temp sensor cannot be initialized");
    err |= ERR_TEMPSEN;
  }
  else {
    // TODO - read temperature and humidity, you probably want to store these as global variables
    temperatureC = p.tempSensor.getTemperatureC();
    humidity = p.tempSensor.getHumidityRH();
    Serial.print("Temperature in C: ");
    Serial.println(temperatureC);
    Serial.print("Humidity: ");
    Serial.println(humidity);
  }

  // check if camera can be initialized
  if (!p.camera.initialize()) {
    Serial.println("TTL Camera cannot be initialized");
    err |= ERR_CAMERA;
  }

  // TODO - implement logAlive() function (notice how RTC and temp sensor just read the necassary data so you don't need to worry about reading those again just store readings as globals)
  logAlive();

  p.initializationSuccess();

  // if current time is outside of operation interval, go to sleep
  if ((err & ERR_RTC) > 0 && !trapActive) {
    p.SleepControl.goToSleep(OFF);
  }

  // set circular buffers and template
  rawSamplesBuffer.setBuffer((uint16_t *)rawSamples, FFT_WINDOW_SIZE, FREQ_WIN_COUNT);
  rawFreqsBuffer.setBuffer((uint16_t *)rawFreqs, FFT_WINDOW_SIZE_BY2, TIME_SMOOTHING);
  processedFreqsBuffer.setBuffer((uint16_t *)processedFreqs, FFT_WINDOW_SIZE_BY2, FREQ_WIN_COUNT);
  correlation.setTemplate((uint16_t *)correlationTemplate, FFT_WINDOW_SIZE_BY2, TEMPLATE_LENGTH, 50, 90);

  // perform calibration (set pre-amp gain) 
  p.amp.powerOn();
  // p.frequencyResponse();
  if (!p.loadSound(p.calibrationFilename)) Serial.printf("loadSound() error: %s\n", p.calibrationFilename);

  //p.calibrate(PREAMP_CALIBRATION, PREAMP_CALIBRATION_THRESH);

  p.preAmp.writeWiperValue(16);

  Wire.end();

  p.amp.powerOff();
  p.Hypnos_5VR_OFF();

  // load playback sound
  if (!p.loadSound(p.playbackFilename)) Serial.printf("loadSound() error: %s\n", p.playbackFilename);
  // end SD
  p.SDCard.end();

  p.Hypnos_3VR_OFF();

  // begin audio sampling
  p.startAudioInput();

}

void loop() {
  // check if audio input buffer is filled (store samples to samples buffer)
  if (!p.audioInputBufferFull(samples)) return;

  updateMicros();

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

  Serial.println(correlationCoefficient);
  // do stuff if correlation is positive...
  if (correlationCoefficient >= CORRELATION_THRESH) {
    // reset correlation count if positive correlatioon didn't occur within CORRELATION_MAX_INTERVAL
    if (microsTime - lastCorrelationTime > CORRELATION_MAX_INTERVAL) correlationCount = 0;
    averagedCorrelationCoefficient[correlationCount] = correlationCoefficient;
    correlationCount += 1;
    lastCorrelationTime = microsTime;
  }
  
  // if count of recent positive correlation is equal to CORRELATION_COUNT, consider this a positive detection
  if (correlationCount == CORRELATION_COUNT) {
    // stop audio sampling
    p.stopAudio();

    Serial.println("Detection occurded!");
    Serial.println("Saving data to SD...");

    correlationCount = 0;
    lastDetectionTime = microsTime;

    // write detection data (structure: DATA/YYMMDD/hhmmss/)
    p.Hypnos_3VR_ON();

    // get date time
    Wire.begin();
    p.RTCWrap.getDateTime().toString(date);
    Serial.println(date);
    Wire.end();

    p.SDCard.begin();
    saveDetection();
    p.SDCard.end();

    p.Hypnos_3VR_OFF();

    // reset buffers
    rawSamplesBuffer.clearBuffer();
    rawFreqsBuffer.clearBuffer();
    processedFreqsBuffer.clearBuffer();

    // Serial.println("Performing playback and audio sampling");
    // restart audio sampling
    p.Hypnos_5VR_ON();
    p.amp.powerOn();

    // p.performPlayback();

    // p.amp.powerOff();
    // p.Hypnos_5VR_OFF();

    //p.startAudioInput();
    p.startAudioInputAndOutput();
  }

  // check if playback duration has exceeded playback duration (turn of simultaneous audio input and output)
  if (microsTime > lastDetectionTime + DETECTION_PLAYBACK_DURATION && p.getAudState() == AUD_STATE::AUD_IN_OUT) {
    p.stopAudio();
    p.startAudioInput();

    p.amp.powerOff();
    p.Hypnos_5VR_OFF();
  } else p.checkResetPlaybackFileIndex();

  // TODO - add other intermittent data logging (see PiedPiper_old.ino as example)
  //        use microsTime here, it stores time returned by micros() everytime the audio input buffer fills
}

// saves detection data to SD card to "/DATA/YYYYMMDD/hhmmss/"
// stores date and time, raw samples, frequency buffers, temperature and humidity data
void saveDetection() {
  char buf[64] = { 0 };
  char buf2[64] = { 0 };

  strcat(buf, "/DATA/");

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
  if (!p.SDCard.openFile(buf, FILE_WRITE)) Serial.printf("openFile() error: %s", buf);
  else {
    p.writeCircularBufferToFile(&processedFreqsBuffer);
    p.SDCard.closeFile();
  }

  // file for raw samples
  strcpy(buf, buf2);
  strcat(buf, "/RAW.TXT");

  // write raw samples buffer to RAW.txt
  if (!p.SDCard.openFile(buf, FILE_WRITE)) Serial.printf("openFile() error: %s", buf);
  else {
    p.writeCircularBufferToFile(&rawSamplesBuffer);
    p.SDCard.closeFile();
  }

  // store details of detection to DETS.txt
  strcpy(buf, buf2);
  strcat(buf, "/DETS.TXT");
  if (!p.SDCard.openFile(buf, FILE_WRITE)) Serial.printf("openFile() error: %s", buf);
  else {
    correlationCoefficient = 0;
    for (int i = 0; i < CORRELATION_COUNT; i++) {
      correlationCoefficient += averagedCorrelationCoefficient[i];
    }
    correlationCoefficient /= CORRELATION_COUNT;
    p.SDCard.data.print(date);
    p.SDCard.data.print(" ");
    p.SDCard.data.print(correlationCoefficient, 3);

    // TODO: add temperature/humidity data here... (consider reading temp sensor data earlier, when reading time from RTC)
    p.SDCard.data.print(temperatureC);
    p.SDCard.data.print(" ");
    p.SDCard.data.print(humidity);
    p.SDCard.data.print(" ");
    
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
}

// TODO: implement logAlive. It must log time from RTC and temperature/humidity data from temperature sensor to LOG.TXT...
//       in this format "YYYYDDMM-hh:mm:ss T H E" (T = temperature, H = humidity, E = err)
//       if rtc is not alive log "YYYYDDMM-hh:mm:ss" instead of time
//       if temperature sensor is not alive, log "T H" instead of temperature and humidity
void logAlive() {
  return;
}

// updates microsTime
void updateMicros() {
  prevMicrosTime = microsTime;
  microsTime = micros();

  // check for overflow, set lastMicrosTime accordingly
  if (microsTime < prevMicrosTime) {
    prevMicrosTime = 0;

    // just to reset audio (in case audio input output was running during overflow)
    p.stopAudio();
    p.startAudioInput();
  }
}
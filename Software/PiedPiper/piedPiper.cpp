#include "piedPiper.h"
#include <String.h>

// Class initialization
piedPiper::piedPiper(void(*audStart)(const unsigned long interval_us, void(*fnPtr)()), void(*audStop)()) {
  ISRStartFn = audStart;
  ISRStopFn = audStop;
}

// Used to stop the interrupt timer for sample recording, so that it does not interfere with SD and Serial communication
void piedPiper::StopAudio()
{
  (*ISRStopFn)();
  //Serial.println("Audio stream stopped");
}

// Used to restart the sample recording timer once SPI and Serial communications have completed.
void piedPiper::StartAudioOutput()
{
  //Serial.println("Restarting audio stream");
  (*ISRStartFn)(outputSampleDelayTime, OutputUpsampledSample);
}

// Used to restart the sample recording timer once SPI and Serial communications have completed.
void piedPiper::StartAudioInput()
{
  //Serial.println("Restarting audio stream");
  if (USE_DETECTION) { (*ISRStartFn)(inputSampleDelayTime, RecordSample); }
}

void piedPiper::init(void) 
{
  // set resolution
  analogReadResolution(ANALOG_RES);
  analogWriteResolution(ANALOG_RES);

  Serial.println("Initializing Pied Piper");

  // Configure pins
  ConfigurePins();

  // Enable the 3 volt rail.
  digitalWrite(HYPNOS_3VR, LOW);

  Serial.println("Testing SD...");

  delay(1000);

  // Verify that SD can be initialized; stop the program if it can't.
  if (!BeginSD())
  {
    Serial.println("SD failed to initialize.");
    initializationFailFlash();
  }

  // read current photo and detection numbers
  ReadPhotoNum();
  ReadDetectionNumber();

  // Print out the current photo and detection numbers.
  Serial.print("Photo number: ");
  Serial.println(GetPhotoNum());
  Serial.print("Detection number: ");
  Serial.println(GetDetectionNum());

  // load settings from SD
  if (!LoadSettings()) {
    Serial.println("LoadSettings() failed");
    initializationFailFlash();
  }

  // Load master template for correlation
  if (!LoadTemplate(templateFilename)) {
    Serial.println("LoadTemplate() failed");
    initializationFailFlash();
  }

  // Load the prerecorded mating call to play back
  if (!LoadSound(playbackFilename)) {
    Serial.println("LoadSound() failed");
    initializationFailFlash();
  }

  SD.end();

  // initialize RTC
  Wire.begin();
  if (!initializeRTC()) {
    Serial.println("initializeRTC() failed");
    // initializationFailFlash();
  }
  Wire.end();
  // Disable the 3-volt rail.
  digitalWrite(HYPNOS_3VR, HIGH);

  // calculate sinc filter tables
  calculateDownsampleSincFilterTable();
  calculateUpsampleSincFilterTable();

  Serial.println("Performing setup playback...");
  Playback();

  // Test the camera module by taking 3 test images
  if (USE_CAMERA_MODULE) {
    for (int i = 0; i < 3; i++)
    {
      if (!TakePhoto(0))
      {
        // Stop the program if the camera module failed to take an image
        Serial.println("Camera module failure!");
        //initializationFailFlash();
      }
      else
      {
        Serial.println("Successfully took test photo.");
      }
    }
  } else { Serial.println("CAMERA MODULE DISABLED"); }

  // Wait here for BEGIN_LOG_WAIT_TIME milliseconds (or until the user sends a character over)
  if (BEGIN_LOG_WAIT_TIME >= 60000) {
    Serial.printf("Send any character to skip the %d minute delay.\n", int(BEGIN_LOG_WAIT_TIME / 60000));
  } else { 
    Serial.printf("Send any character to skip the %d second delay.\n", int(BEGIN_LOG_WAIT_TIME / 1000));
  }

  long ct = millis();
  while (millis() - ct < BEGIN_LOG_WAIT_TIME)
  {
    if (Serial.available())
    {
      while (Serial.available())
      {
        Serial.read();
      }
      break;
    }
  }

  ResetFrequencyBuffers();

  StartAudioInput();
  if (!USE_DETECTION) {
    StopAudio();
    Serial.println("AUDIO SAMPLING DISABLED");
  }
}

bool piedPiper::initializeRTC() {
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
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
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


// This records new audio samples into the sample buffer asyncronously using a hardware timer.
// Before recording a sample, it first checks if the buffer is full.
// If it is full, it does not record a sample.
// If it is not full, it records a sample and moves the pointer over.
void piedPiper::RecordSample(void)
{
  if (inputSampleBufferIdx < FFT_WIN_SIZE)
  {
    // store last location in input buffer and read sample into circular downsampling input buffer
    int downsampleInputIdxCpy = downsampleInputIdx;
    downsampleFilterInput[downsampleInputIdx++] = analogRead(AUD_IN);

    if (downsampleInputIdx == sincTableSizeDown) downsampleInputIdx = 0;
    downsampleInputCount++;
    // performs downsampling every AUD_IN_DOWNSAMPLE_RATIO samples
    if (downsampleInputCount == AUD_IN_DOWNSAMPLE_RATIO) {
      downsampleInputCount = 0;
      // calculate downsampled value using sinc filter table
      float downsampleSample = 0.0;
      for (int i = 0; i < sincTableSizeDown; i++) {
        downsampleSample += downsampleFilterInput[downsampleInputIdxCpy++] * sincFilterTableDownsample[i];
        if (downsampleInputIdxCpy == sincTableSizeDown) downsampleInputIdxCpy = 0;
      }
      // store downsampled value in input sample buffer
      inputSampleBuffer[inputSampleBufferIdx++] = round(downsampleSample);
    }
  }
}

// Function called by the interrupt timer to output an audio sample.
// It used cubic interpolation to increase the output sampling frequency to a high enough rate
// to eliminate high-frequency noise that would otherwise make it through the single-stage
// analog filter at the input of the amplifier driving the vibration exciter.
void piedPiper::OutputSample(void) {
  analogWrite(AUD_OUT, nextOutputSample);

  interpCount++;
  if (outputSampleBufferIdx < (playbackSampleCount - 2))
  {
    if (!(interpCount < AUD_OUT_INTERP_RATIO))
    {
      interpCount = 0;
      outputSampleBufferIdx++;

      interpCoeffA = -0.5 * outputSampleBuffer[outputSampleBufferIdx - 1] + 1.5 * outputSampleBuffer[outputSampleBufferIdx] - 1.5 * outputSampleBuffer[outputSampleBufferIdx + 1] + 0.5 * outputSampleBuffer[outputSampleBufferIdx + 2];
      interpCoeffB = outputSampleBuffer[outputSampleBufferIdx - 1] - 2.5 * outputSampleBuffer[outputSampleBufferIdx] + 2 * outputSampleBuffer[outputSampleBufferIdx + 1] - 0.5 * outputSampleBuffer[outputSampleBufferIdx + 2];
      interpCoeffC = -0.5 * outputSampleBuffer[outputSampleBufferIdx - 1] + 0.5 * outputSampleBuffer[outputSampleBufferIdx + 1];
      interpCoeffD = outputSampleBuffer[outputSampleBufferIdx];
    }

    float t = (interpCount * 1.0) / AUD_OUT_INTERP_RATIO;

    nextOutputSample = max(0, min(4095, round(interpCoeffA * t * t * t + interpCoeffB * t * t + interpCoeffC * t + interpCoeffD)));
  }
}

void piedPiper::OutputUpsampledSample(void) {
  // write sample to AUD_OUT
  analogWrite(AUD_OUT, nextOutputSample);

  // return if outside of outputSampleBuffer bounds
  if (outputSampleBufferIdx == playbackSampleCount) return;
  // Otherwise, calculate next upsampled value for AUD_OUT

  // store last location of upsampling input buffer
  int upsampleInputIdxCpy = upsampleInputIdx;
  
  // store value of sample to filter input buffer when upsample count == 0, otherwise pad with zeroes
  upsampleFilterInput[upsampleInputIdx++] = upsampleInputCount++ > 0 ? 0 : outputSampleBuffer[outputSampleBufferIdx++];
  
  if (upsampleInputCount == AUD_OUT_UPSAMPLE_RATIO) upsampleInputCount = 0;
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

// Determines if the sample buffer is full, and is ready to have frequencies computed.
bool piedPiper::InputSampleBufferFull()
{
  return !(inputSampleBufferIdx < FFT_WIN_SIZE);
}

// Loads sound data from the SD card into the audio output buffer. Once loaded, the data will
// remain in-place until LoadSound is called again. This means that all future calls to 
// Playback will play back the same audio data.
bool piedPiper::LoadSound(char* fname) {
  //Serial.println("SD initialized successfully, checking directory...");

  if (!OpenFile(fname, FILE_READ)) {
    digitalWrite(HYPNOS_3VR, HIGH);
    // StartAudioInput();
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

  return true;
}

// Called when the input sample buffer is full, to incorperate the new data into the large "cold" frequency array
// It clears the input buffer (by just resetting the pointer to the start of the array)
// It performs time and frequency smoothing on the new data
// It adds the new frequency block to the large frequency array.
void piedPiper::ProcessData()
{
  //Quickly pull data from volatile buffer into sample buffer, iterating pointer as needed.
  for (int i = 0; i < FFT_WIN_SIZE; i++)
  {
    samples[samplePtr] = inputSampleBuffer[i];
    samplePtr = IterateCircularBufferPtr(samplePtr, sampleCount);
    vReal[i] = inputSampleBuffer[i];
    vImag[i] = 0.0;
  }

  inputSampleBufferIdx = 0;

  //Calculate FFT of new data and put into time smoothing buffer
  FFT.DCRemoval(vReal, FFT_WIN_SIZE); // Remove DC component of signal
  FFT.Windowing(vReal, FFT_WIN_SIZE, FFT_WIN_TYP_HAMMING, FFT_FORWARD); // Apply windowing function to data
  FFT.Compute(vReal, vImag, FFT_WIN_SIZE, FFT_FORWARD); // Compute FFT
  FFT.ComplexToMagnitude(vReal, vImag, FFT_WIN_SIZE); // Compute frequency magnitudes

  for (int i = 0; i < FFT_WIN_SIZE_BY2; i++) {
    vReal[i] *= FREQ_WIDTH;
  }

  // alpha trimming step...
  AlphaTrimming(ALPHA_TRIM_WINDOW, ALPHA_TRIM_THRESH);

  // store raw frequency magnitudes to rawFreqs
  for (int i = 0; i < FFT_WIN_SIZE_BY2; i++)
  {
    rawFreqs[rawFreqsPtr][i] = vReal[i];
    vReal[i] = 0.0;
  }

  rawFreqsPtr = IterateCircularBufferPtr(rawFreqsPtr, TIME_AVG_WIN_COUNT);

  // use vReal as scratch space to store time averaged data of rawFreqs
  float timeAvgDiv = 1.0 / TIME_AVG_WIN_COUNT;
  for (int t = 0; t < TIME_AVG_WIN_COUNT; t++)
  {
    for (int f = 0; f < FFT_WIN_SIZE_BY2; f++) {
      vReal[f] += rawFreqs[t][f] * timeAvgDiv;
    }
  }

  SmoothFreqs(FREQ_SMOOTHING);

  // store smoothed time-averaged data into freqs
  for (int f = 0; f < FFT_WIN_SIZE_BY2; f++)
  {
    freqs[freqsPtr][f] = round(vReal[f]);
  }

  // SmoothFreqs(16);

  // // subtract further smoothed data from data in freqs to help remove background noise
  // for (int f = 0; f < FFT_WIN_SIZE_BY2; f++)
  // {
  //   // instead of subtracting, would it make sense to check if (freqs[freqsPtr][f] < vReal[f] * (some_threshold))
  //   // and zero freqs[freqsPtr][f] if true.
  //   // Is it possible that freqs[freqPtr][f] can be negative?
  //   freqs[freqsPtr][f] = freqs[freqsPtr][f] - round(vReal[f]);
  // }

  freqsPtr = IterateCircularBufferPtr(freqsPtr, freqWinCount);
}

// Performs noise subtraction utilizing alpha trimming
void piedPiper::AlphaTrimming(int winSize, float threshold)
{
  float subtractionData[FFT_WIN_SIZE_BY2];
  for (int i = 0; i < FFT_WIN_SIZE_BY2; i++) {
    subtractionData[i] = vReal[i];
  }

  int upperBound = 0;
  int lowerBound = 0;

  int winSizeBy2 = winSize / 2;
  for (int i = 0; i < FFT_WIN_SIZE_BY2; i++) {
    lowerBound = max(0, i - winSizeBy2);
    upperBound = min(FFT_WIN_SIZE_BY2 - 1, i + winSizeBy2);
    
    int boundAvgCount = 0;
    float boundAvg = 0;

    // get average of magnitudes within lower and upper bound
    for (int k = lowerBound; k <= upperBound; k++) {
      boundAvg += vReal[k];
      boundAvgCount += 1;
    }

    boundAvgCount = boundAvgCount > 0 ? boundAvgCount : 1;
    boundAvg /= boundAvgCount;

    float boundStdDev = 0;
    // get standard deviation of magnitudes within lower and upper bound
    for (int k = lowerBound; k <= upperBound; k++) {
      boundStdDev += pow(vReal[k] - boundAvg, 2);
    }
    //Serial.println(boundStdDev)

    // standard deviation 
    boundStdDev = sqrt(boundStdDev / boundAvgCount);

    // check deviation of samples within lower and upper bound
    for (int k = lowerBound; k <= upperBound; k++) {
      // if deviation of sample is above threshold
      if ((vReal[k] - boundAvg) / boundStdDev > threshold) {
        // replace this sample in subtractionData with the average excluding this sample
        float trimmedAvg = 0;
        int trimmedAvgCount = 0;
        for (int j = lowerBound; j <= upperBound; j++) {
          if (j == k) continue;
          trimmedAvg += vReal[j];
          trimmedAvgCount += 1;
        }
        // ensure to not divide by 0
        trimmedAvgCount = trimmedAvgCount > 0 ? trimmedAvgCount : 1;
        subtractionData[k] = trimmedAvg / trimmedAvgCount;
      } 
    }
  }

  // subtraction
  for (int i = 0; i < FFT_WIN_SIZE_BY2; i++) {
    vReal[i] -= subtractionData[i];
    //Serial.println(vReal[i]);
  }
}

// Performs rectangular smoothing on frequency data stored in [vReal].
void piedPiper::SmoothFreqs(int winSize)
{
  float inptDup[FFT_WIN_SIZE_BY2];
  int upperBound = 0;
  int lowerBound = 0;
  int avgCount = 0;
  int winSizeBy2 = winSize / 2;

  for (int i = 0; i < FFT_WIN_SIZE_BY2; i++)
  {
    inptDup[i] = vReal[i];
    vReal[i] = 0;
  }

  for (int i = 0; i < FFT_WIN_SIZE_BY2; i++)
  {
    lowerBound = max(0, i - winSizeBy2);
    upperBound = min(FFT_WIN_SIZE_BY2 - 1, i + winSizeBy2);
    avgCount = 0;

    for (int v = lowerBound; v <= upperBound; v++)
    {
      vReal[i] += inptDup[v];
      avgCount++;
    }

    vReal[i] /= avgCount;
  }
}

// load template and store into array... init fail flash if template doesn't exist or template is invalid (size does not match)
// When template data is collected, it must be ran through the exact same processing as frequency data for best results.
// Template data collection must be done through the device, perhaps a seperate ino file will be good for this,
// but in the future the Pied Piper library will allow configuring device through settings file on SD card.

// Load template from SD card and store into template array, performs initial computation.
// Once loaded, the data will remain in-place until LoadTemplate is called again
bool piedPiper::LoadTemplate(char *fname) {
  // load template from SD card and store into template array
  if (!OpenFile(fname, FILE_READ)) {
    // digitalWrite(HYPNOS_3VR, HIGH);
    return false;
  }

  while(data.available()) {
    int f = 0;
    float sample = 0.0;
    for (int t = 0; t < TEMPLATE_LENGTH; t++) {
      for (f = 0; f < FFT_WIN_SIZE_BY2; f++) {
        sample = data.readStringUntil('\n').toFloat();
        templateData[t][f] = int(round(sample * FREQ_WIDTH));
      }
    }
  }

  data.close();

  // compute square root of the sum of the template squared
  templateSqrtSumSq = 0;
  long long m = 0;
  for (int t = 0; t < TEMPLATE_LENGTH; t++) {
    for (int f = FREQ_MARGIN_LOW; f < FREQ_MARGIN_HIGH; f++) {
      m = templateData[t][f];
      templateSqrtSumSq += m * m;
    }
  }
  // store sqaure root of template sum squared
  templateSqrtSumSq = sqrt(templateSqrtSumSq);

  inverseTemplateSqrtSumSq = 1.0 / templateSqrtSumSq;

  return true;
}

// do cross correlation between template and processed frequency data... return correlation coefficient
float piedPiper::CrossCorrelation() {
  long long freqsSqrtSumSq = 0;

  // compute square root of the sum squared of the current freqs
  long long m = 0;

  // start correlation at latest freqs window (freqsPtr - 1)
  //int currentFreqsPos = freqsPtr == 0 ? freqWinCount - 1 : freqsPtr - 1;
  int currentFreqsPos = (freqsPtr - TEMPLATE_LENGTH + freqWinCount) % freqWinCount;
  //Serial.println(currentFreqsPos);
  
  int tempT = currentFreqsPos;

  for (int t = 0; t < TEMPLATE_LENGTH; t++) {
    for (int f = FREQ_MARGIN_LOW; f < FREQ_MARGIN_HIGH; f++) {
      m = freqs[tempT][f];
      freqsSqrtSumSq += m * m;
    }
    tempT = IterateCircularBufferPtr(tempT, freqWinCount);
  }

  // storing inverse of the product of square root sum squared of template and freqs (done to reduce use of division)
  freqsSqrtSumSq = sqrt(freqsSqrtSumSq) * templateSqrtSumSq;
  // ensuring to not divide by 0
  float InverseSqrtSumSq = freqsSqrtSumSq > 0 ? 1.0 / freqsSqrtSumSq : 1.0;

  tempT = currentFreqsPos;
  // computing dot product and correlation coefficient
  float correlationCoefficient = 0.0;
  for (int t = 0; t < TEMPLATE_LENGTH; t++) {
    for (int f = FREQ_MARGIN_LOW; f < FREQ_MARGIN_HIGH; f++) {
      correlationCoefficient += templateData[t][f] * freqs[tempT][f] * InverseSqrtSumSq;
    }
    tempT = IterateCircularBufferPtr(tempT, freqWinCount);
  }

  //Serial.println(correlationCoefficient);
  
  return correlationCoefficient;
}


// calculates sinc filter table for downsampling a signal by @ratio
// @nz is the number of zeroes to use for the table
void piedPiper::calculateDownsampleSincFilterTable() {
  int ratio = AUD_IN_DOWNSAMPLE_RATIO;
  int nz = AUD_IN_DOWNSAMPLE_FILTER_ZERO_X;
  // Build sinc function table for downsampling by @AUD_IN_DOWNSAMPLE_RATIO
  int n = sincTableSizeDown;

  // stores time values corresponding to sinc function
  float ns[n];
  float ns_step = float(nz * ratio * 2) / (n - 1);

  // stores time values corresponding to 1 period of cosine function for windowing the sinc function
  float t[n];
  float t_step = 1.0 / (n - 1);

  for (int i = 0; i < n; i++) {
    // calculate time values for sinc function, [-nz * ratio to nz * ratio] spaced apart by ns_step
    ns[i] = float(-1.0 * nz * ratio) + ns_step * i;
    // calculate time values for cosine function, [0.0 to 1.0] spaced apart by t_step
    t[i] = t_step * i;
  }
  // ensure to not divide by 0
  ns[round((n - 1) / 2.0)] = 1.0;

  // calculate sinc function and store in table
  for (int i = 0; i < n; i++) {
    sincFilterTableDownsample[i] = (1.0 / ratio) * sin(PI * ns[i] / ratio) / (PI * ns[i] / ratio);
  }

  // sinc function is 'undefined' at 0 (sinc(0)/0), therefore set to 1.0 / ratio
  sincFilterTableDownsample[int(round((n - 1) / 2.0))] = 1.0 / ratio;

  // window sinc function table with cosine wave 
  for (int i = 0; i < n; i++) {
    sincFilterTableDownsample[i] = sincFilterTableDownsample[i] * 0.5 * (1.0 - cos(2.0 * PI * t[i]));
  }
}

// calculates sinc filter table for upsampling a signal by @ratio
// @nz is the number of zeroes to use for the table
void piedPiper::calculateUpsampleSincFilterTable() {
  int ratio = AUD_OUT_UPSAMPLE_RATIO;
  int nz = AUD_OUT_UPSAMPLE_FILTER_ZERO_X;
  // Build sinc function table for upsampling by @upsample_ratio
  int n = sincTableSizeUp;

  // stores time values corresponding to sinc function
  float ns[n];
  float ns_step = float(nz * 2) / (n - 1);

  // stores time values corresponding to 1 period of cosine function for windowing the sinc function
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

// This tests the cold frequency array for the conditions required for a positive detection.
// A positive detection is made when at least [THRESHHOLD] frequency peaks of [THRESHHOLD] magnitudes are found in the target frequency harmonics.
bool piedPiper::InsectDetection()
{
  int count = 0;

  // Determine if there are target frequency peaks at every window in the frequency data array
  for (int t = 0; t < freqWinCount; t++)
  {
    if (CheckFreqDomain(t)) count++;
  }

  //Serial.println(count);
  // If the number of windows that contain target frequency peaks is at or above the expected number that would be present
  return (count >= EXP_SIGNAL_LEN * (freqWinCount / REC_TIME) * EXP_DET_EFF);
}

// Determines if there is a peak in the target frequency range at a specific time window (including harmonics).
bool piedPiper::CheckFreqDomain(int t)
{
  int lowerIdx = floor(((TGT_FREQ - FREQ_MARGIN) * 1.0) / FFT_SAMPLE_FREQ * FFT_WIN_SIZE);
  int upperIdx = ceil(((TGT_FREQ + FREQ_MARGIN) * 1.0) / FFT_SAMPLE_FREQ * FFT_WIN_SIZE);

  int count = 0;

  //const float scaledSigThresh = SIG_THRESH * FFT_WINDOW_SIZE / 2;

  // check fundamental frequency at first iteration, and for harmonics in next iterations
  for (int h = 1; h <= HARMONICS; h++)
  {
    // check if fundamental frequency (TGT_FREQ) +- FREQ_MARGIN contains a peak
    for (int i = (h * lowerIdx); i <= (h * upperIdx); i++)
    {
      // checks if current magnitude is a peak, by comparing with previous and next magnitudes
      if (((freqs[t][i + 1] - freqs[t][i]) < 0) && ((freqs[t][i] - freqs[t][i - 1]) > 0))
      {
        // check if magnitude is above SIG_THRESH, increment count if so
        if (freqs[t][i] > SIG_THRESH)
        {
          count++;
          continue;
        }
      }
    }
  }

  return (count > 0);
}

// Records detection data to the uSD card, and clears all audio buffers (both sample & frequency data)
void piedPiper::SaveDetection()
{
  // StopAudio();
  ResetSPI();
  digitalWrite(HYPNOS_3VR, LOW);

  lastDetectionTime = millis();
  detectionNum++;

  //Serial.println("Positive detection");

  if(!BeginSD())
  {
    digitalWrite(HYPNOS_3VR, HIGH);
    // StartAudioInput();
    return;
  }

  Wire.begin();

  DateTime currentDT = rtc.now();
  data = SD.open("/DATA/DETS/DETS.txt", FILE_WRITE);
  data.print(detectionNum);
  data.print(",");
  data.print(currentDT.year(), DEC);
  data.print(",");
  data.print(currentDT.month(), DEC);
  data.print(",");
  data.print(currentDT.day(), DEC);
  data.print(",");
  data.print(currentDT.hour(), DEC);
  data.print(",");
  data.print(currentDT.minute(), DEC);
  data.print(",");
  data.println(currentDT.second(), DEC);
  data.close();

  Wire.end();

  // Creates data file that stores the raw input data responsible for the detection.

  char dir[24] = "/DATA/DETS/";
  char str[10];
  itoa(detectionNum, str, 10);
  strcat(dir, str);
  strcat(dir, ".txt");

  data = SD.open(dir, FILE_WRITE);

  for (int i = 0; i < sampleCount; i++)
  {
    data.println(samples[samplePtr], DEC);
    samples[samplePtr] = 0;
    samplePtr = IterateCircularBufferPtr(samplePtr, sampleCount);
  }

  samplePtr = 0;

  data.close();

  // Creates a detection data file that stores the processed frequency data responsible for the detection.

  char dir2[24] = "/DATA/DETS/F";
  char str2[10];
  itoa(detectionNum, str2, 10);
  strcat(dir2, str2);
  strcat(dir2, ".txt");

  data = SD.open(dir2, FILE_WRITE);

  for (int t = 0; t < freqWinCount; t++)
  {
    for (int f = 0; f < FFT_WIN_SIZE_BY2; f++)
    {
      data.print(freqs[freqsPtr][f], DEC);
      data.print(",");

    }

    freqsPtr = IterateCircularBufferPtr(freqsPtr, freqWinCount);
    data.println();
  }

  data.close();

  SD.end();

  digitalWrite(HYPNOS_3VR, HIGH);

  //Serial.println("Done saving detection");
}

// reset frequency buffers and audio input
void piedPiper::ResetFrequencyBuffers()
{
  for (int t = 0; t < freqWinCount; t++)
  {
    for (int f = 0; f < FFT_WIN_SIZE_BY2; f++)
    {
      freqs[t][f] = 0;
    }
  }

  // freqsPtr = 0;

  for (int t = 0; t < TIME_AVG_WIN_COUNT; t++)
  {
    for (int f = 0; f < FFT_WIN_SIZE_BY2; f++)
    {
      rawFreqs[t][f] = 0;
    }
  }

  // rawFreqsPtr = 0;

  // reset input buffer index
  //inputSampleBufferIdx = 0;
}

bool piedPiper::TakePhotoTTL()
{
  return 0;
}

// Takes a single photo, and records what time and detection it corresponds to
bool piedPiper::TakePhoto(int n)
{
  if (USE_CAMERA_MODULE == 0) { return true; }
  
  // StopAudio();


  lastImgTime = millis();

  uint8_t vid, pid;
  uint8_t temp;

  photoNum++;


  // Powers on and initializes SPI and I2C ===================================
  digitalWrite(HYPNOS_5VR, HIGH);
  digitalWrite(HYPNOS_3VR, LOW);
  digitalWrite(CAM_CS, HIGH);

  ResetSPI();

  SPI.begin();
  Wire.begin();

  delay(20);

  // Resets camera CPLD ===========================================
  CameraModule.write_reg(0x07, 0x80);
  delay(100);

  CameraModule.write_reg(0x07, 0x00);
  delay(100);

  // Verify that SPI communication with the camera has successfully begun.
  //Check if the ArduCAM SPI bus is OK
  CameraModule.write_reg(ARDUCHIP_TEST1, 0x55);
  temp = CameraModule.read_reg(ARDUCHIP_TEST1);
  if (temp != 0x55)
  {
    Serial.println(F("SPI interface Error!"));
    digitalWrite(HYPNOS_5VR, LOW);
    digitalWrite(HYPNOS_3VR, HIGH);
    digitalWrite(CAM_CS, LOW);
    SPI.end();
    Wire.end();
    // StartAudioInput();
    return false;
  }
  else
  {
    //Serial.println(F("SPI interface OK."));
  }

  // Verify that the camera module is alive and well ==========================

  CameraModule.wrSensorReg8_8(0xff, 0x01);
  CameraModule.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
  CameraModule.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
  if ((vid != 0x26 ) && (( pid != 0x41 ) || ( pid != 0x42 )))
  {
    Serial.println(F("ACK CMD Can't find OV2640 module!"));
    digitalWrite(HYPNOS_5VR, LOW);
    digitalWrite(HYPNOS_3VR, HIGH);
    digitalWrite(CAM_CS, LOW);
    SPI.end();
    Wire.end();
    // StartAudioInput();
    return false;
  }
  else
  {
    //Serial.println(F("ACK CMD OV2640 detected."));
  }

  // Reduce SPI clock speed for stability =======================================

  delay(10);

  // COnfigure camera module ==================================================
  CameraModule.set_format(JPEG);
  CameraModule.InitCAM();
  CameraModule.clear_fifo_flag();
  CameraModule.write_reg(ARDUCHIP_FRAMES, 0x00);
  CameraModule.OV2640_set_JPEG_size(OV2640_1600x1200);

  CameraModule.flush_fifo();
  delay(10);

  CameraModule.clear_fifo_flag();

  delay(4000);

  // Capture an image =======================================================
  CameraModule.start_capture();

  //Serial.println("Begun capture, waiting for reply.");

  //Serial.println(F("start capture."));
  long photoStartTime = millis();

  // Wait for the image capture to be complete =============================
  while ( !CameraModule.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
    delay(10);
    if (millis() - photoStartTime > 5000)
    {
      Serial.println("Failed to take photo.");
      digitalWrite(HYPNOS_5VR, LOW);
      digitalWrite(HYPNOS_3VR, HIGH);
      digitalWrite(CAM_CS, LOW);
      SPI.end();
      Wire.end();
      StartAudioInput();
      return false;
    }
  }

  //Serial.println("Recieved reply");

  //SPI.endTransaction();
  //SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));

  delay(10);

  // Initialize SD Card ========================================================
  if (!BeginSD())
  {
    Serial.println("SD failed to begin.");
    digitalWrite(HYPNOS_5VR, LOW);
    digitalWrite(HYPNOS_3VR, HIGH);
    digitalWrite(CAM_CS, LOW);
    Wire.end();
    SPI.end();
    StartAudioInput();
    return false;
  }

  //Serial.println(F("CAM Capture Done."));

  // Read image from camera and store on uSD card. ==========================
  read_fifo_burst(CameraModule);

  //Serial.println("Image saved");

  //Clear the capture done flag ====================================
  CameraModule.clear_fifo_flag();
  delay(10);

  // The format is: [detection #], [year], [month], [day], [hour], [minute], [second]
  DateTime currentDT = rtc.now();
  data = SD.open("/DATA/PHOTO/PHOTO.txt", FILE_WRITE);
  data.print(n);
  data.print(",");
  data.print(currentDT.year(), DEC);
  data.print(",");
  data.print(currentDT.month(), DEC);
  data.print(",");
  data.print(currentDT.day(), DEC);
  data.print(",");
  data.print(currentDT.hour(), DEC);
  data.print(",");
  data.print(currentDT.minute(), DEC);
  data.print(",");
  data.println(currentDT.second(), DEC);
  data.close();

  SD.end();
  SPI.end();
  Wire.end();

  digitalWrite(HYPNOS_5VR, LOW);
  digitalWrite(HYPNOS_3VR, HIGH);

  digitalWrite(CAM_CS, LOW);

  for (int i = 0; i < freqWinCount; i++) {
    for (int f = 0; f < FFT_WIN_SIZE_BY2; f++) {
      freqs[i][f] = 0;
    }
  }

  // StartAudioInput();
  return true;
  
}

uint8_t piedPiper::read_fifo_burst(ArduCAM CameraModule)
{
  bool is_header = false;
  uint8_t temp = 0, temp_last = 0;
  uint32_t length = 0;
  static int i = 0;
  static int k = 0;
  char dir[24] = "/DATA/PHOTO/";
  File outFile;
  byte buf[256];



  length = CameraModule.read_fifo_length();

  //Serial.print(F("The fifo length is :"));
  //Serial.println(length, DEC);

  if (length >= MAX_FIFO_SIZE) //8M
  {
    //Serial.println("Over size.");
    return 0;
  }

  if (length == 0 ) //0 kb
  {
    //Serial.println(F("Size is 0."));
    return 0;
  }

  digitalWrite(CAM_CS, LOW);
  CameraModule.set_fifo_burst();//Set fifo burst mode
  i = 0;

  while ( length-- )
  {
    temp_last = temp;
    temp =  SPI.transfer(0x00);

    //End of the image transmission
    if ( (temp == 0xD9) && (temp_last == 0xFF) ) //If find the end ,break while,
    {
      buf[i++] = temp;  //save the last  0XD9

      //Write the remain bytes in the buffer
      digitalWrite(CAM_CS, HIGH);
      outFile.write(buf, i);

      //Close the file
      outFile.close();
      //Serial.print(F("OK: "));
      //Serial.println(length);

      is_header = false;
      digitalWrite(CAM_CS, LOW);
      CameraModule.set_fifo_burst();
      i = 0;
    }

    if (is_header == true)
    {
      //Write image data to buffer if not full
      if (i < 256)
        buf[i++] = temp;
      else
      {
        //Write 256 bytes image data to file
        digitalWrite(CAM_CS, HIGH);
        outFile.write(buf, 256);
        i = 0;
        buf[i++] = temp;
        digitalWrite(CAM_CS, LOW);
        CameraModule.set_fifo_burst();
      }
    }

    // Beginning of the image transmission
    else if ((temp == 0xD8) & (temp_last == 0xFF)) // This is the start of a jpg file header
    {
      is_header = true;
      digitalWrite(CAM_CS, HIGH);

      //Assembles filename
      char str[10];
      itoa(photoNum, str, 10);
      strcat(dir, str);
      strcat(dir, ".jpg");

      //Open the new file
      outFile = SD.open(dir, O_WRITE | O_CREAT | O_TRUNC);

      if (! outFile)
      {
        Serial.println(F("File open failed"));
        return 0;
      }

      digitalWrite(CAM_CS, LOW);
      CameraModule.set_fifo_burst();
      buf[i++] = temp_last;
      buf[i++] = temp;
    }
  }

  digitalWrite(CAM_CS, HIGH);
  return 1;
}

// Plays back a female mating call using the vibration exciter
void piedPiper::Playback() {
  // StopAudio();

  //Serial.println("Enabling amplifier...");

  digitalWrite(HYPNOS_5VR, HIGH);
  analogWrite(AUD_OUT, 2048);
  digitalWrite(AMP_SD, HIGH);
  delay(100);

  //Serial.println("Amplifier enabled. Beginning playback ...");

  StartAudioOutput();

  while (outputSampleBufferIdx < playbackSampleCount) {}

  StopAudio();

  //Serial.println("Finished playback ISR.\nShutting down amplifier...");

  outputSampleBufferIdx = 0;
  outputSampleInterpCount = 0;

  digitalWrite(AMP_SD, LOW);
  analogWrite(AUD_OUT, 0);
  digitalWrite(HYPNOS_5VR, LOW);


  //Serial.println("Amplifer shut down.");
  lastPlaybackTime = millis();

  // StartAudioInput();
}

// Records the most recent time that the unit is alive.
void piedPiper::LogAlive()
{
  // StopAudio();
  lastLogTime = millis();
  ResetSPI();
  digitalWrite(HYPNOS_3VR, LOW);

  if (!BeginSD())
  {
    digitalWrite(HYPNOS_3VR, HIGH);
    StartAudioInput();
    return;
  }

  data = SD.open("LOG.TXT", FILE_WRITE);

  data.println(lastLogTime, DEC);

  data.close();

  SD.end();
  digitalWrite(HYPNOS_3VR, HIGH);
  // StartAudioInput();
}

/**
 * Log the current time to gauge how long the device has been alive in the field
 */
void piedPiper::LogAliveRTC() {
  ResetSPI();
  digitalWrite(HYPNOS_3VR, LOW);

  if (!SD.begin(SD_CS)) {
    digitalWrite(HYPNOS_3VR, HIGH);
    return;
  }

  // open file for writing, OpenFile will create LOG.txt if it does not exist
  if (!OpenFile("LOG.txt", FILE_WRITE)) {
    return;
  }

  char date[] = "YYMMDD-hh:mm:ss";

  Wire.begin();
  rtc.now().toString(date);
  Wire.end();
  
  data.println(date);

  Serial.println(date);
  
  data.close();

  SD.end();
  digitalWrite(HYPNOS_3VR, HIGH);
}

//+
int piedPiper::IterateCircularBufferPtr(int currentVal, int arrSize)
{
  if ((currentVal + 1) == arrSize)
  {
    return 0;
  }
  else
  {
    return (currentVal + 1);
  }
}


unsigned long piedPiper::GetLastLogTime()
{
  return lastLogTime;
}

unsigned long piedPiper::GetLastDetectionTime()
{
  return lastDetectionTime;
}

unsigned long piedPiper::GetLastPhotoTime()
{
  return lastImgTime;
}

unsigned long piedPiper::GetLastPlaybackTime()
{
  return lastPlaybackTime;
}

int piedPiper::GetDetectionNum()
{
  return detectionNum;
}

void piedPiper::ConfigurePins()
{
  pinMode(AUD_IN, INPUT);
  pinMode(AUD_OUT, OUTPUT);
  pinMode(HYPNOS_5VR, OUTPUT);
  pinMode(HYPNOS_3VR, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  pinMode(CAM_CS, OUTPUT);
  pinMode(AMP_SD, OUTPUT);
}

void piedPiper::ResetSPI()
{
  pinMode(23, INPUT);
  pinMode(24, INPUT);
  pinMode(10, INPUT);

  delay(20);

  pinMode(23, OUTPUT);
  pinMode(24, OUTPUT);
  pinMode(10, OUTPUT);
}

bool piedPiper::BeginSD()
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

bool piedPiper::OpenFile(char *fname, uint8_t mode) {
  // check if file exists (also check only if mode == FILE_READ)
  if (mode == FILE_READ && !SD.exists(fname)) return false;

  // open file in mode
  data = SD.open(fname, mode);

  // check if file was opened successfully
  if (!data) return false;

  // return true on success
  return true;
}

bool piedPiper::LoadSettings() {

  if (!OpenFile((char *)settingsFilename, FILE_READ)) {
    digitalWrite(HYPNOS_3VR, HIGH);
    return false;
  }

  while (data.available()) {
    String settingName = data.readStringUntil(':');
    data.read();
    String setting = data.readStringUntil('\n');
    setting.trim();

    // store to corresponding setting on device
    if (settingName == "playback") {
      strcpy(playbackFilename, "/PBAUD/");
      strcat(playbackFilename, setting.c_str());  
    } else if (settingName == "template") {
      strcpy(templateFilename, "/TEMPS/");
      strcat(templateFilename, setting.c_str());
    } else if (settingName == "operation") {
      strcpy(operationTimesFilename, "/PBINT/");
      strcat(operationTimesFilename, setting.c_str());
    } else continue;
  }

  data.close();

  return true;
}

void piedPiper::ReadPhotoNum() {
  // Set current photo number
  int pn = 1;
  char str[10];

  // Count the number of images inside /DATA/PHOTO/
  while (1)
  {
    char dir[32];
    strcpy(dir, "/DATA/PHOTO/");
    itoa(pn, str, 10);
    strcat(dir, str);
    strcat(dir, ".jpg");

    //Serial.println(dir);

    if (SD.exists(dir)) pn++;
    else break;
  }

    // Set the photo number to match the number of photos inside the directory.
    SetPhotoNum(pn - 1);
}

void piedPiper::ReadDetectionNumber() {
  // Set detection number
  int dn = 1;
  char str[10];

  // Count the number of detections inside /DATA/DETS/
  while (1)
  {
    char dir[32];
    strcpy(dir, "/DATA/DETS/");
    itoa(dn, str, 10);
    strcat(dir, str);
    strcat(dir, ".txt");

    if (SD.exists(dir)) dn++;
    else break;
  }
  
  // Set the detection number to match the number of detections inside the directory.
  SetDetectionNum(dn - 1);
}



void piedPiper::ResetOperationIntervals()
{
  lastLogTime = 0;
  lastDetectionTime = 0;
  lastImgTime = 0;
  lastPlaybackTime = 0;
}

void piedPiper::SetPhotoNum(int n)
{
  photoNum = n;
}

int piedPiper::GetPhotoNum()
{
  return photoNum;
}

void piedPiper::SetDetectionNum(int n)
{
  detectionNum = n;
}

// If something fails to initialize, stop the program and execute this function that repeatedly blinks
// the camera flash to show that there's a problem.
void piedPiper::initializationFailFlash()
{
  while (1)
  {
    digitalWrite(HYPNOS_3VR, LOW);
    delay(500);
    digitalWrite(HYPNOS_3VR, HIGH);
    delay(500);
  };
}

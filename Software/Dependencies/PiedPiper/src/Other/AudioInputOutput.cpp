#include "../PiedPiper.h"

// audio input buffer
volatile uint16_t AUD_IN_BUFFER[FFT_WINDOW_SIZE];
volatile uint16_t AUD_IN_BUFFER_IDX = 0;

// audio output buffer
uint16_t PiedPiperBase::PLAYBACK_FILE[SAMPLE_RATE * PLAYBACK_FILE_LENGTH];
uint16_t PiedPiperBase::PLAYBACK_FILE_SAMPLE_COUNT;

volatile uint16_t PLAYBACK_FILE_BUFFER_IDX = 0;

AUD_STATE PiedPiperBase::audState = AUD_STATE::AUD_STOP;

// calculating sinc table sizes
const int sincTableSizeDown = (2 * SINC_FILTER_DOWNSAMPLE_ZERO_X + 1) * AUD_IN_DOWNSAMPLE_RATIO - AUD_IN_DOWNSAMPLE_RATIO + 1;
const int sincTableSizeUp = (2 * SINC_FILTER_UPSAMPLE_ZERO_X + 1) * AUD_OUT_UPSAMPLE_RATIO - AUD_OUT_UPSAMPLE_RATIO + 1;

// tables holding values corresponding to sinc filter for band limited upsampling/downsampling
float sincFilterTableDownsample[sincTableSizeDown];
float sincFilterTableUpsample[sincTableSizeUp];

// circular input buffer for downsampling
volatile uint16_t downsampleFilterInput[sincTableSizeDown];
volatile uint16_t downsampleInputIdx = 0;
volatile uint16_t downsampleInputCount = 0;

// circular input buffer for upsampling
volatile uint16_t upsampleFilterInput[sincTableSizeUp];
volatile uint16_t upsampleInputIdx = 0;
volatile uint16_t upsampleInputCount = 0;

// table holding values computed to flatten frequency response
float flatteningFilter[WINDOW_SIZE];
volatile uint16_t flatteningFilterInput[WINDOW_SIZE];
volatile uint16_t flatteningInputIdx = 0;

volatile float filteredValue = 0.0;

volatile uint16_t nextOutputSample = 0;

volatile uint16_t sampleCount = 0;

void PiedPiperBase::generateImpulse() {
    for (uint16_t i = 0; i < WINDOW_SIZE; i++) {
        flatteningFilter[i] = 0.0;
    }
    flatteningFilter[FFT_WINDOW_SIZE] = 1.0;
}

void PiedPiperBase::RESET_PLAYBACK_FILE_INDEX() { PLAYBACK_FILE_BUFFER_IDX = 0; }

void PiedPiperBase::checkResetPlaybackFileIndex() {
    if (PLAYBACK_FILE_BUFFER_IDX >= PLAYBACK_FILE_SAMPLE_COUNT) PLAYBACK_FILE_BUFFER_IDX = 0;
}

uint16_t PiedPiperBase::getPlaybackFileIndex() {
    return PLAYBACK_FILE_BUFFER_IDX;
}

void PiedPiperBase::calculateDownsampleSincFilterTable(void) {
    int ratio = AUD_IN_DOWNSAMPLE_RATIO;
    int nz = SINC_FILTER_DOWNSAMPLE_ZERO_X;
    // Build sinc function table for bandlimited downsampling by @AUD_IN_DOWNSAMPLE_RATIO
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

void PiedPiperBase::calculateUpsampleSincFilterTable(void) {
    int ratio = AUD_OUT_UPSAMPLE_RATIO;
    int nz = SINC_FILTER_UPSAMPLE_ZERO_X;
    // Build sinc function table for band limited upsampling by ratio
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

void PiedPiperBase::RecordAndOutputSample(void) {
    OutputSample();
    sampleCount += 1;
    if (sampleCount < AUD_OUT_UPSAMPLE_RATIO) return;
    
    sampleCount = 0;
    RecordSample();
}

void PiedPiperBase::RecordAndOutputRawSample(void) {
    if (AUD_IN_BUFFER_IDX >= FFT_WINDOW_SIZE) return;
    analogWrite(PIN_AUD_OUT, PLAYBACK_FILE[PLAYBACK_FILE_BUFFER_IDX]);
    AUD_IN_BUFFER[AUD_IN_BUFFER_IDX] = analogRead(PIN_AUD_IN);
    
    AUD_IN_BUFFER_IDX += 1;
    PLAYBACK_FILE_BUFFER_IDX += 1;
}

void PiedPiperBase::RecordSample(void) {
    if (AUD_IN_BUFFER_IDX >= FFT_WINDOW_SIZE) return;
    // store last location in input buffer and read sample into circular downsampling input buffer
    static int downsampleInputIdxCpy = downsampleInputIdx;
    downsampleFilterInput[downsampleInputIdx++] = analogRead(PIN_AUD_IN);

    if (downsampleInputIdx == sincTableSizeDown) downsampleInputIdx = 0;
    downsampleInputCount++;
    // performs downsampling every AUD_IN_DOWNSAMPLE_RATIO samples
    if (downsampleInputCount == AUD_IN_DOWNSAMPLE_RATIO) {
        downsampleInputCount = 0;
        // calculate downsampled value using sinc filter table
        filteredValue = 0.0;
        for (uint16_t i = 0; i < sincTableSizeDown; i++) {
            filteredValue += downsampleFilterInput[downsampleInputIdxCpy++] * sincFilterTableDownsample[i];
            if (downsampleInputIdxCpy == sincTableSizeDown) downsampleInputIdxCpy = 0;
        }
        // store downsampled value in input sample buffer
        AUD_IN_BUFFER[AUD_IN_BUFFER_IDX++] = int(round(filteredValue));
    }
}

void PiedPiperBase::OutputSample(void) {
    // write sample to AUD_OUT
    analogWrite(PIN_AUD_OUT, nextOutputSample);

    // return if outside of outputSampleBuffer bounds
    if (PLAYBACK_FILE_BUFFER_IDX >= PLAYBACK_FILE_SAMPLE_COUNT) return;
    // Otherwise, calculate next upsampled value for AUD_OUT

    // First layer of convolution - playback signal frequency response flattening
    filteredValue = 0.0;

    if (upsampleInputCount == 0) {
        static uint16_t flatteningInputIdxCpy = flatteningInputIdx;
        flatteningFilterInput[flatteningInputIdx++] = PLAYBACK_FILE[PLAYBACK_FILE_BUFFER_IDX++];

        if (flatteningInputIdx == WINDOW_SIZE) flatteningInputIdx = 0;

        // convolute filter input with reciprocal of recorded frequency response
        for (uint16_t i = 0; i < WINDOW_SIZE; i++) {
            filteredValue += flatteningFilterInput[flatteningInputIdxCpy++] * flatteningFilter[i];
            if (flatteningInputIdxCpy == WINDOW_SIZE) flatteningInputIdxCpy = 0;
        }
    }

    upsampleInputCount += 1;

    // Second layer of convolution - upsampling flattened playback signal
    // store last location of upsampling input buffer
    static uint16_t upsampleInputIdxCpy = upsampleInputIdx;

    // store value of sample to filter input buffer when upsample count == 0, otherwise pad with zeroes
    upsampleFilterInput[upsampleInputIdx++] = round(filteredValue);

    if (upsampleInputCount == AUD_OUT_UPSAMPLE_RATIO) upsampleInputCount = 0;
    if (upsampleInputIdx == sincTableSizeUp) upsampleInputIdx = 0;

    // calculate upsampled value
    filteredValue = 0.0;
    // convolute filter input with sinc function
    for (uint16_t i = 0; i < sincTableSizeUp; i++) {
        filteredValue += upsampleFilterInput[upsampleInputIdxCpy++] * sincFilterTableUpsample[i];
        if (upsampleInputIdxCpy == sincTableSizeUp) upsampleInputIdxCpy = 0;
    }

    // copy filtered value to next output sample
    nextOutputSample = max(0, min(DAC_MAX, int(round(filteredValue))));
}

bool PiedPiperBase::audioInputBufferFull(uint16_t *bufferPtr) {
    if (!(AUD_IN_BUFFER_IDX < FFT_WINDOW_SIZE)) {
        for (int i = 0; i < FFT_WINDOW_SIZE; i++) {
            bufferPtr[i] = AUD_IN_BUFFER[i];
        }
        AUD_IN_BUFFER_IDX = 0;
        return true;
    }
    return false;

}

void PiedPiperBase::startAudioInput() {
    TimerInterrupt.attachTimerInterrupt(AUD_IN_SAMPLE_DELAY_TIME, RecordSample);
    audState = AUD_STATE::AUD_IN;
}

void PiedPiperBase::startAudioInputAndOutput() {
    TimerInterrupt.attachTimerInterrupt(AUD_OUT_SAMPLE_DELAY_TIME, RecordAndOutputSample);
    audState = AUD_STATE::AUD_IN_OUT;
}

void PiedPiperBase::startRawAudioInputAndOutput() {
    TimerInterrupt.attachTimerInterrupt(AUD_IN_SAMPLE_DELAY_TIME, RecordAndOutputRawSample);
    audState = AUD_STATE::AUD_IN_OUT;
}

void PiedPiperBase::stopAudio() {
    TimerInterrupt.detachTimerInterrupt();
    audState = AUD_STATE::AUD_STOP;
}

AUD_STATE PiedPiperBase::getAudState() {
    return audState;
}

void PiedPiperBase::impulseResponseCalibration(uint8_t responseAveraging) {
    // used for storing time domain samples and computing FFT
    uint16_t _rawSamples[FFT_WINDOW_SIZE];
    complex _samples[WINDOW_SIZE];
    complex _averagedFFT[WINDOW_SIZE];

    uint8_t _averageCount = 0;
    float _inverseAverage = 1.0 / responseAveraging;

    uint16_t i = 0;

    // setting up impulse for playback
    for (i = 0; i < WINDOW_SIZE; i++) {
        PLAYBACK_FILE[i] = 0;
        _averagedFFT[i] = 0;
    }
    PLAYBACK_FILE[0] = DAC_MAX;

    PLAYBACK_FILE_SAMPLE_COUNT = WINDOW_SIZE;

    // clearing sample buffer
    startAudioInput();

    while (!audioInputBufferFull(_rawSamples))
        ;

    stopAudio();

    // averaging frequency response of impulse recordings
    while (_averageCount < responseAveraging) {
        // starting playback and recording of impulse...adding slight delay between recordings
        delay(500);

        // restore playback index
        RESET_PLAYBACK_FILE_INDEX();

        // starting raw sampling and audio output
        startRawAudioInputAndOutput();

        // due to the size of volatile buffer (WINDOW_SIZE / AUD_IN_DOWNSAMPLE_RATIO) 
        // two raw windows are needed to get a WINDOW_SIZE of samples
        while (!audioInputBufferFull(_rawSamples))
            ;

        // storing samples to first half of FFT input array
        for (i = 0; i < FFT_WINDOW_SIZE; i++) {
            _samples[i] = _rawSamples[i];
        }

        // sampling second window...
        while (!audioInputBufferFull(_rawSamples))
            ;

        stopAudio();

        // storing samples to second half of FFT input array
        for (i = 0; i < FFT_WINDOW_SIZE; i++) {
            _samples[i + FFT_WINDOW_SIZE] = _rawSamples[i];
        }

        // removing dc noise from recording
        DCRemoval(_samples, WINDOW_SIZE);

        // running FFT on recorded impulse data
        Fast4::FFT(_samples, WINDOW_SIZE);

        // averaging and storing frequency response
        for (i = 0; i < WINDOW_SIZE; i++) {
            _averagedFFT[i] += _samples[i] * _inverseAverage;
        }

        _averageCount += 1;
    }
    
    // computing inverse of averaged frequency response
    for (i = 0; i < WINDOW_SIZE; i++) {
        if (_averagedFFT[i] != 0) _averagedFFT[i] = 1.0 / _averagedFFT[i];
    }
    _averagedFFT[0] = 0.0;

    // computing time domain signal corresponding to inverse frequency response
    Fast4::IFFT(_averagedFFT, WINDOW_SIZE);

    // windowing signal with cosine window and getting sum to ensure sum of signal is equal to 1.0
    float _sum = 0;
    float _cosTime = 2 * PI / (WINDOW_SIZE - 1);

    for (i = 0; i < WINDOW_SIZE; i++) {
        flatteningFilter[i] = _averagedFFT[i].re() * 0.5 * (1.0 - cos(_cosTime * i));
        _sum += flatteningFilter[i];
    }

    if (_sum == 0) _sum = 1.0;

    for (i = 0; i < WINDOW_SIZE; i++) {
        flatteningFilter[i] /= _sum;
        // Serial.print(flatteningFilter[i], 8);
        // Serial.print(", ");
    }
    // Serial.println();

}
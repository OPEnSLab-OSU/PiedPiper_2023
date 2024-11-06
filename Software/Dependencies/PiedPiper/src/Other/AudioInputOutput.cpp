#include "../PiedPiper.h"

// audio input buffer
volatile uint16_t AUD_IN_BUFFER[FFT_WINDOW_SIZE];
volatile uint16_t AUD_IN_BUFFER_IDX = 0;

// audio output buffer
uint16_t PiedPiperBase::PLAYBACK_FILE[SAMPLE_RATE * PLAYBACK_FILE_LENGTH];
uint16_t PiedPiperBase::PLAYBACK_FILE_SAMPLE_COUNT;

volatile uint16_t PiedPiperBase::PLAYBACK_FILE_BUFFER_IDX = 0;

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

volatile float filteredValue = 0.0;

volatile uint16_t nextOutputSample = 0;

void PiedPiperBase::RESET_PLAYBACK_FILE_INDEX() { PLAYBACK_FILE_BUFFER_IDX = 0; }

void PiedPiperBase::calculateDownsampleSincFilterTable(void) {
    int ratio = AUD_IN_DOWNSAMPLE_RATIO;
    int nz = SINC_FILTER_DOWNSAMPLE_ZERO_X;
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

void PiedPiperBase::calculateUpsampleSincFilterTable(void) {
    int ratio = AUD_OUT_UPSAMPLE_RATIO;
    int nz = SINC_FILTER_UPSAMPLE_ZERO_X;
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


volatile uint16_t sampleCount = 0;

void PiedPiperBase::RecordAndOutputSample(void) {
    OutputSample();
    sampleCount += 1;
    if (sampleCount == AUD_OUT_UPSAMPLE_RATIO) {
        sampleCount = 0;
        RecordSample();
    }
}

void PiedPiperBase::RecordSample(void) {
    if (AUD_IN_BUFFER_IDX >= FFT_WINDOW_SIZE) return;
    // store last location in input buffer and read sample into circular downsampling input buffer
    int downsampleInputIdxCpy = downsampleInputIdx;
    downsampleFilterInput[downsampleInputIdx++] = analogRead(PIN_AUD_IN);

    if (downsampleInputIdx == sincTableSizeDown) downsampleInputIdx = 0;
    downsampleInputCount++;
    // performs downsampling every AUD_IN_DOWNSAMPLE_RATIO samples
    if (downsampleInputCount == AUD_IN_DOWNSAMPLE_RATIO) {
        downsampleInputCount = 0;
        // calculate downsampled value using sinc filter table
        filteredValue = 0.0;
        for (int i = 0; i < sincTableSizeDown; i++) {
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

    // store last location of upsampling input buffer
    int upsampleInputIdxCpy = upsampleInputIdx;

    // store value of sample to filter input buffer when upsample count == 0, otherwise pad with zeroes
    upsampleFilterInput[upsampleInputIdx++] = upsampleInputCount++ > 0 ? 0 : PLAYBACK_FILE[PLAYBACK_FILE_BUFFER_IDX++];

    if (upsampleInputCount == AUD_OUT_UPSAMPLE_RATIO) upsampleInputCount = 0;
    if (upsampleInputIdx == sincTableSizeUp) upsampleInputIdx = 0;

    // calculate upsampled value
    filteredValue = 0.0;
    // convolute filter input with sinc function
    for (int i = 0; i < sincTableSizeUp; i++) {
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
}

void PiedPiperBase::startAudioInputAndOutput() {
    TimerInterrupt.attachTimerInterrupt(AUD_OUT_SAMPLE_DELAY_TIME, RecordAndOutputSample);
}

void PiedPiperBase::stopAudio() {
    TimerInterrupt.detachTimerInterrupt();
}
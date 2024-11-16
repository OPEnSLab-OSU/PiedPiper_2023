#include "PiedPiper.h"

PiedPiperMonitor::PiedPiperMonitor() {

}

void PiedPiperMonitor::init() {
    PiedPiperBase::init();

}

void PiedPiperMonitor::frequencyResponse() {
    // char outFile[] = "CALTD.txt";
    // char outFile2[] = "CALFD.txt";
    uint16_t samples[FFT_WINDOW_SIZE];
    float vReal[FFT_WINDOW_SIZE];
    float vImag[FFT_WINDOW_SIZE];

    for (uint16_t i = 0; i < FFT_WINDOW_SIZE; i++) {
        PLAYBACK_FILE[i] = DAC_MID;
    }
    PLAYBACK_FILE[FFT_WINDOW_SIZE_BY2] = DAC_MAX;

    PLAYBACK_FILE_SAMPLE_COUNT = 256;

    RESET_PLAYBACK_FILE_INDEX();

    analogWrite(PIN_AUD_OUT, 2048);

    startAudioInputAndOutput();

    while (!audioInputBufferFull(samples));

    stopAudio();

    for (int i = 0; i < FFT_WINDOW_SIZE; i++) {
        vReal[i] = samples[i];
        vImag[i] = 0;
    }

    // SDCard.openFile(outFile, FILE_WRITE);
    // writeArrayToFile<uint16_t>(samples, FFT_WINDOW_SIZE);
    // SDCard.closeFile();

    analogWrite(PIN_AUD_OUT, 0);

    FFT(vReal, vImag, FFT_WINDOW_SIZE);

    for (int i = 0; i < FFT_WINDOW_SIZE_BY2; i++) {
        vReal[i] = 1.0 / vReal[i];
    }

    // SDCard.openFile(outFile2, FILE_WRITE);
    // writeArrayToFile<float>(vReal, FFT_WINDOW_SIZE_BY2);
    // SDCard.closeFile();

    
}

void PiedPiperMonitor::calibrate(uint16_t calibrationValue, uint16_t threshold) {

    uint16_t _playbackNumWindows = round(PLAYBACK_FILE_SAMPLE_COUNT / WINDOW_SIZE);
    uint16_t _calValLow, _calValHigh, _windowCount, i;
    int16_t _nextPotValue, _nextPotValueBy2, _lastPotValue;
    uint16_t _max;
    uint16_t _temp[FFT_WINDOW_SIZE];

    _calValLow = calibrationValue - threshold;
    _calValHigh = calibrationValue + threshold;
    _nextPotValue = 256;
    _nextPotValueBy2 = 256;
    _windowCount = 0;

    analogWrite(PIN_AUD_OUT, 2048);

    delay(1000);

    startAudioInput();

    while (!audioInputBufferFull(_temp))
        ;

    stopAudio();

    while (1) {

        _max = 0;
        _windowCount = 0;

        if (preAmp.writeWiperValue(_nextPotValue) > 0) Serial.println("writeWiperValue() error");

        delay(500);

        RESET_PLAYBACK_FILE_INDEX();

        startAudioInputAndOutput();

        while (_windowCount < _playbackNumWindows) {
            if (!audioInputBufferFull(_temp)) continue;

            // Serial.println(_windowCount);
            _windowCount += 1;

            for (i = 0; i < FFT_WINDOW_SIZE; i++) {
                if (_temp[i] > _max) _max = _temp[i];
            }
        }

        stopAudio();

        // analogWrite(PIN_AUD_OUT, 2048);

        _lastPotValue = _nextPotValue;

        _nextPotValueBy2 = _nextPotValueBy2 >> 1;
        _nextPotValueBy2 = _nextPotValueBy2 > 0 ? _nextPotValueBy2 : 1;

        if (_max > _calValHigh) _nextPotValue = _nextPotValue + _nextPotValueBy2;
        else if (_max < _calValLow) _nextPotValue = _nextPotValue -  _nextPotValueBy2;
        else break;

        _nextPotValue = max(0, min(256, _nextPotValue));

        // Serial.print("next wiper value: ");
        // Serial.println(_nextPotValue);

        Serial.print("readWiperValue(): ");
        Serial.println(preAmp.readWiperValue());

        if (_nextPotValue == _lastPotValue) break;

    }

}

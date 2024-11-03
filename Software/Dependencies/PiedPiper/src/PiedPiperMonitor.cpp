#include "PiedPiper.h"

PiedPiperMonitor::PiedPiperMonitor() {

}

void PiedPiperMonitor::init() {
    PiedPiperBase::init();
}

void PiedPiperMonitor::calibrate(uint16_t calibrationValue, uint16_t threshold) {

    uint16_t _playbackNumWindows = round(PLAYBACK_FILE_SAMPLE_COUNT / WINDOW_SIZE);
    uint16_t _calValLow, _calValHigh, _nextPotValue, _nextPotValueBy2, _windowCount, i;
    float _max;
    float _temp[FFT_WINDOW_SIZE];

    _calValLow = calibrationValue - threshold;
    _calValHigh = calibrationValue + threshold;
    _nextPotValue = 256;
    _nextPotValueBy2 = _nextPotValue;
    _windowCount = 0;

    // Hypnos_5VR_ON();

    // Wire.begin();
    // preAmp.writeWiperValue(_nextPotValue);
    // Wire.end();

    // amp.powerOn();

    analogWrite(PIN_AUD_OUT, 2048);

    while (1) {

        _max = 0;
        _windowCount = 0;

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

        analogWrite(PIN_AUD_OUT, 0);

        _nextPotValueBy2 = _nextPotValueBy2 >> 1;
        _nextPotValueBy2 = _nextPotValueBy2 > 0 ? _nextPotValueBy2 : 1;

        if (_max > _calValHigh) _nextPotValue = max(0, _nextPotValue - _nextPotValueBy2);
        else if (_max < _calValLow) _nextPotValue = min(256, _nextPotValue  + _nextPotValueBy2);
        else break;

        Serial.println(_nextPotValue);

        // Wire.begin();

        // preAmp.writeWiperValue(_nextPotValue);

        // Wire.end();

    }

    // amp.powerOff();
    // Hypnos_5VR_OFF();

}

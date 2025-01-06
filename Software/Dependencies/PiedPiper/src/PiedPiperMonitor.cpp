#include "PiedPiper.h"

DFRobot_SHT3x sht31 = DFRobot_SHT3x(&Wire, DEFAULT_SHT31_ADDR, -1);

PiedPiperMonitor::PiedPiperMonitor() {}

void PiedPiperMonitor::init() {
    PiedPiperBase::init();
    this->tempSensor = &sht31;
}

bool PiedPiperMonitor::preAmpGainCalibration(float adcRange, float rangeThreshold) {

    // length of calibration file in windows
    uint16_t _playbackNumWindows = round(PLAYBACK_FILE_SAMPLE_COUNT / WINDOW_SIZE);
    
    // a bunch of temporary variables for digital pot adjustment
    uint16_t _calValLow, _calValHigh, _max, _sample;
    int16_t _nextWiperValue, _nextWiperValueBy2, _lastWiperValue;

    uint16_t _samples[FFT_WINDOW_SIZE]; // array for storing recorded samples

    uint16_t _windowCount, i;

    uint32_t _sum;

    // computing calibration thresholds
    _calValLow = round(adcRange * ADC_MID) - round(rangeThreshold * ADC_MID);
    _calValHigh = round(adcRange * ADC_MID) + round(rangeThreshold * ADC_MID);

    // initial position of digital pot wiper
    _nextWiperValue = 256;
    _nextWiperValueBy2 = _nextWiperValue;

    //if (preAmp.writeWiperValue(_nextWiperValue) > 0) Serial.println("writeWiperValue() error");

    // starting calibration procedure
    analogWrite(PIN_AUD_OUT, 2048);

    delay(1000);

    // clearing sample buffer
    startAudioInput();

    while (!audioInputBufferFull(_samples))
        ;

    stopAudio();

    // sampling playback signal and recording peak amplitude value to adjust digital pot
    while (1) {

        _max = 0;
        _windowCount = 0;

        // writing next wiper value to digital pot
        if (preAmp.writeWiperValue(_nextWiperValue) > 0) Serial.println("writeWiperValue() error");
        // adding slight delay for things to stabalize
        delay(500);

        RESET_PLAYBACK_FILE_INDEX();

        // start audio input and output
        startAudioInputAndOutput();

        // waiting for full duration of calibration sound to be completed
        while (_windowCount < _playbackNumWindows) {
            if (!audioInputBufferFull(_samples)) continue;

            // removing DC component from recorded signal
            _sum = 0;
            for (i = 0; i < FFT_WINDOW_SIZE; i++) {
                _sum += _samples[i];
            }
            _sum /= FFT_WINDOW_SIZE;

            // finding maximum value in recorded samples
            for (i = 0; i < FFT_WINDOW_SIZE; i++) {
                _sample = abs(int(_samples[i]) - _sum);
                if (_sample > _max) _max = _sample;
            }

            _windowCount += 1;
        }

        stopAudio();

        _lastWiperValue = _nextWiperValue;

        // incremental adjustment of wiper value in powers of 2 (256 -> 128 ... 2 -> 1)
        _nextWiperValueBy2 = _nextWiperValueBy2 >> 1;
        _nextWiperValueBy2 = _nextWiperValueBy2 > 0 ? _nextWiperValueBy2 : 1;

        // checking which direction to adjust wiper, no adjustment needed if value is within range
        if (_max > _calValHigh) _nextWiperValue = _nextWiperValue + _nextWiperValueBy2;
        else if (_max < _calValLow) _nextWiperValue = _nextWiperValue -  _nextWiperValueBy2;
        else break;

        // ensuring next wiper value is digital pot range
        _nextWiperValue = max(0, min(256, _nextWiperValue));

        // Serial.print("next wiper value: ");
        // Serial.println(_nextWiperValue);

        // Serial.print("readWiperValue(): ");
        // Serial.println(preAmp.readWiperValue());

        // break if the next wiper value and previous wiper value are equal (i.e both are 0 or 256) this indicates that calibration has failed
        if (_nextWiperValue == _lastWiperValue) return false;

    }
    // return true on success
    return true;

}
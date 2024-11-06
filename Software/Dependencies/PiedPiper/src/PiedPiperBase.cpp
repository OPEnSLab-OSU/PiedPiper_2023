#include "PiedPiper.h"

PiedPiperBase::PiedPiperBase() {}

// uint16_t PiedPiperBase::PLAYBACK_FILE[SAMPLE_RATE * PLAYBACK_FILE_LENGTH];
// uint16_t PiedPiperBase::PLAYBACK_FILE_SAMPLE_COUNT;

void PiedPiperBase::init() {
    this->configurePins();
    this->calculateDownsampleSincFilterTable();
    this->calculateUpsampleSincFilterTable();
    this->TimerInterrupt.initialize();
    delay(1000);
}

void PiedPiperBase::configurePins(void) {
    pinMode(PIN_HYPNOS_3VR, OUTPUT);
    pinMode(PIN_HYPNOS_5VR, OUTPUT);
    pinMode(PIN_SD_CS, OUTPUT);
    pinMode(PIN_AMP_SD, OUTPUT);
    pinMode(PIN_AUD_OUT, OUTPUT);
    pinMode(PIN_AUD_IN, INPUT);
    pinMode(PIN_RTC_ALARM, INPUT_PULLUP);

    analogReadResolution(ADC_RESOLUTION);
    analogWriteResolution(DAC_RESOLUTION);
}

void PiedPiperBase::Hypnos_3VR_ON() { digitalWrite(PIN_HYPNOS_3VR, LOW); }

void PiedPiperBase::Hypnos_3VR_OFF() { digitalWrite(PIN_HYPNOS_3VR, HIGH); }

void PiedPiperBase::Hypnos_5VR_ON() { digitalWrite(PIN_HYPNOS_5VR, HIGH); }

void PiedPiperBase::Hypnos_5VR_OFF() { digitalWrite(PIN_HYPNOS_5VR, LOW); }

void PiedPiperBase::initializationFail() {
    indicator.begin();
    indicator.clear();

    WDTControl.start();

    while(1) {
        indicator.setPixelColor(0, 255, 0, 0);
        indicator.show();
        delay(500);
        indicator.setPixelColor(0, 0, 0, 0);
        indicator.show();
        delay(500);
    }
}

void PiedPiperBase::initializationSuccess() {
    indicator.begin();
    indicator.clear();

    delay(500);
    indicator.setPixelColor(0, 0, 255, 0);
    indicator.show();
    delay(500);
    indicator.setPixelColor(0, 0, 0, 0);
    indicator.show();
    delay(500);
    indicator.clear();    
}

bool PiedPiperBase::loadSettings(char *filename) {
    if (!SDCard.openFile(filename, FILE_READ)) return false;

    while (SDCard.data.available()) {
        String settingName = SDCard.data.readStringUntil(':');
        SDCard.data.read();
        String setting = SDCard.data.readStringUntil('\n');
        setting.trim();

        // store to corresponding setting on device
        if (settingName == "calibration") {
            strcpy(this->calibrationFilename, "/PBAUD/");
            strcat(this->calibrationFilename, setting.c_str());  
        } else if (settingName == "playback") {
            strcpy(this->playbackFilename, "/PBAUD/");
            strcat(this->playbackFilename, setting.c_str());  
        } else if (settingName == "template") {
            strcpy(this->templateFilename, "/TEMPS/");
            strcat(this->templateFilename, setting.c_str());
        } else if (settingName == "operation") {
            strcpy(this->operationTimesFilename, "/PBINT/");
            strcat(this->operationTimesFilename, setting.c_str());
        } else continue;
    }

    SDCard.closeFile();

    return true;
}

bool PiedPiperBase::loadSound(char *filename) {
    if (!SDCard.openFile(filename, FILE_READ)) return false;

    //Serial.println("Opened file. Loading samples...");

    // get size of playback file (in bytes)
    int fsize = SDCard.data.size();

    // playback file is exported as 16-bit unsigned int, the total number of samples stored in the file is fsize / 2
    PLAYBACK_FILE_SAMPLE_COUNT = fsize / 2;

    for (int i = 0; i < PLAYBACK_FILE_SAMPLE_COUNT; i++) {
        // stop reading at end of file, and ensure 2 bytes are available for reading
        if ((2 * i) >= (fsize - 1)) break;
        // read two bytes at a time and store to outputSampleBuffer[i]
        SDCard.data.read(&PLAYBACK_FILE[i], 2);
        // Serial.println(PLAYBACK_FILE[i]);
    }

    SDCard.closeFile();

    return true;
}

bool PiedPiperBase::loadTemplate(char *filename, uint16_t *bufferPtr, uint16_t templateLength) {
    if (!SDCard.openFile(filename, FILE_READ)) return false;

    int t, f;

    float sample;

    while(SDCard.data.available()) {
        for (t = 0; t < templateLength; t++) {
            for (f = 0; f < FFT_WINDOW_SIZE_BY2; f++) {
                sample = SDCard.data.readStringUntil('\n').toFloat();
                *((bufferPtr + f) + t * FFT_WINDOW_SIZE_BY2) = uint16_t(round(sample * FREQ_WIDTH));
            }
        }
    }

    SDCard.closeFile();

    return true;
}

bool PiedPiperBase::loadOperationTimes(char *filename) {
    if (!SDCard.openFile(filename, FILE_READ)) return false;

    uint8_t hour = 0;
    uint8_t minute = 0;
    // read and store playback intervals
    while(SDCard.data.available()) {
        // data in operation intervals file is stored in the following manner (XX:XX - XX:XX)
        // read hour from file (read until colon)
        hour = SDCard.data.readStringUntil(':').toInt();
        // read minute
        minute = SDCard.data.readStringUntil(' ').toInt();
        OperationMan.addOperationTime(hour, minute);
        // read until start of end time
        SDCard.data.readStringUntil(' ');
        // read hour and minute
        hour = SDCard.data.readStringUntil(':').toInt();
        minute = SDCard.data.readStringUntil('\n').toInt();
        OperationMan.addOperationTime(hour, minute);
    }

    SDCard.closeFile();

    // SD.end();
    // digitalWrite(HYPNOS_3VR, HIGH);

    // print playback intervals
    // Serial.println("Printing operation hours...");
    // for (int i = 0; i < numOperationTimes; i = i + 2) {
    // Serial.printf("\t%02d:%02d - %02d:%02d\n", operationTimes[i].hour, operationTimes[i].minute, operationTimes[i + 1].hour, operationTimes[i + 1].minute);
    // }
    // Serial.println();

    return true;
}

// template <typename T>
// void PiedPiperBase::writeArrayToFile(T *array, uint16_t arrayLength) {
//     for (int i = 0; i < arrayLength; i++) {
//         SDCard.data.println(array[i], DEC);
//     }
// }

// template <typename T>
// void PiedPiperBase::writeCircularBufferToFile(CircularBuffer<T> *buffer) {
//     uint16_t _rows = buffer->getNumRows();
//     uint16_t _cols = buffer->getNumCols();
//     T *_column = 0;
//     for (int i = 1; i <= _cols; i++) {
//         _column = buffer->getData(i);
//         for (int j = 0; j < _rows; j++) {
//             SDCard.data.println(_column[j], DEC);
//         }
//     }
// }

void PiedPiperBase::performPlayback() {
    stopAudio();

    analogWrite(PIN_AUD_OUT, DAC_MID);
    
    TimerInterrupt.attachTimerInterrupt(AUD_OUT_SAMPLE_DELAY_TIME, OutputSample);

    while (PLAYBACK_FILE_BUFFER_IDX < PLAYBACK_FILE_SAMPLE_COUNT);

    stopAudio();

    RESET_PLAYBACK_FILE_INDEX();

    analogWrite(PIN_AUD_OUT, 0);
}
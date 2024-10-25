#include "PiedPiper.h"

char PiedPiperBase::playbackFilename[32];
char PiedPiperBase::templateFilename[32];
char PiedPiperBase::operationTimesFilename[32];

uint16_t PiedPiperBase::PLAYBACK_FILE[SAMPLE_RATE * PLAYBACK_FILE_LENGTH];
uint16_t PiedPiperBase::PLAYBACK_FILE_SAMPLE_COUNT;

void PiedPiperBase::Hypnos_3VR_ON() { digitalWrite(PIN_HYPNOS_3VR, LOW); }

void PiedPiperBase::Hypnos_3VR_OFF() { digitalWrite(PIN_HYPNOS_3VR, HIGH); }

void PiedPiperBase::Hypnos_5VR_ON() { digitalWrite(PIN_HYPNOS_5VR, HIGH); }

void PiedPiperBase::Hypnos_5VR_OFF() { digitalWrite(PIN_HYPNOS_5VR, LOW); }

void PiedPiperBase::initializationFail() {
    this->indicator.begin();
    this->indicator.clear();

    this->WDT.start();

    while(1) {
        this->indicator.setPixelColor(0, 255, 0, 0);
        this->indicator.show();
        delay(500);
        this->indicator.setPixelColor(0, 0, 0, 0);
        this->indicator.show();
        delay(500);
    }
}

void PiedPiperBase::initializationSuccess() {
    this->indicator.begin();
    this->indicator.clear();

    delay(500);
    this->indicator.setPixelColor(0, 0, 255, 0);
    this->indicator.show();
    delay(500);
    this->indicator.setPixelColor(0, 0, 0, 0);
    this->indicator.show();
    delay(500);
    this->indicator.clear();    
}

bool PiedPiperBase::loadSettings(char *filename) {
    if (!this->SDCard.OpenFile(filename, FILE_READ)) return false;

    while (this->SDCard.data.available()) {
        String settingName = this->SDCard.data.readStringUntil(':');
        this->SDCard.data.read();
        String setting = this->SDCard.data.readStringUntil('\n');
        setting.trim();

        // store to corresponding setting on device
        if (settingName == "playback") {
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

    this->SDCard.closeFile();

    return true;
}

bool PiedPiperBase::loadSound(char *filename) {
    if (!this->SDCard.OpenFile(filename, FILE_READ)) return false;

    //Serial.println("Opened file. Loading samples...");

    // get size of playback file (in bytes)
    int fsize = this->SDCard.data.size();

    // playback file is exported as 16-bit unsigned int, the total number of samples stored in the file is fsize / 2
    this->PLAYBACK_FILE_SAMPLE_COUNT = fsize / 2;

    for (int i = 0; i < PLAYBACK_FILE_SAMPLE_COUNT; i++) {
        // stop reading at end of file, and ensure 2 bytes are available for reading
        if ((2 * i) >= (fsize - 1)) break;
        // read two bytes at a time and store to outputSampleBuffer[i]
        this->SDCard.data.read(&this->PLAYBACK_FILE[i], 2);
    }

    this->SDCard.closeFile();

    return true;
}

bool PiedPiperBase::loadTemplate(char *filename, uint16_t *bufferPtr, uint16_t templateLength) {
    if (!this->SDCard.OpenFile(filename, FILE_READ)) return false;

    int t, f;

    while(this->SDCard.data.available()) {
        for (t = 0; t < templateLength; t++) {
            for (f = 0; f < FFT_WINDOW_SIZE_BY2; f++) {
                *((this->bufferPtr + f) + t * FFT_WINDOW_SIZE_BY2) = this->SDCard.data.readStringUntil('\n').toInt();
            }
        }
    }

    this->SDCard.closeFile();

    return true;
}

bool PiedPiperBase::loadOperationTimes(char *filename) {
    if (!this->SDCard.OpenFile(filename, FILE_READ)) return false;

    uint8_t hour = 0;
    uint8_t minute = 0;
    // read and store playback intervals
    while(this->SDCard.data.available()) {
        // data in operation intervals file is stored in the following manner (XX:XX - XX:XX)
        // read hour from file (read until colon)
        hour = this->SDCard.data.readStringUntil(':').toInt();
        // read minute
        minute = this->SDCard.data.readStringUntil(' ').toInt();
        this->operationManager.addOperationTime(hour, minute);
        // read until start of end time
        this->SDCard.data.readStringUntil(' ');
        // read hour and minute
        hour = this->SDCard.data.readStringUntil(':').toInt();
        minute = this->SDCard.data.readStringUntil('\n').toInt();
        this->operationManager.addOperationTime(hour, minute);
    }

    this->SDCard.closeFile();

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
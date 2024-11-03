#include "Peripherals.h"

SDWrapper::SDWrapper(uint8_t PIN_CS) {
    this->PIN_CS = PIN_CS;
}

bool SDWrapper::initialize(void) {
    bool _returnVal = SD.begin(this->PIN_CS);
    SD.end();
    return _returnVal;
}

bool SDWrapper::begin(void) {
    return SD.begin(this->PIN_CS);
}

void SDWrapper::end(void) {
    SD.end();
}

bool SDWrapper::openFile(char *filename, uint8_t mode) {
    if (mode == FILE_READ && !SD.exists(filename)) return false;

    // open file in mode
    this->data = SD.open(filename, mode);

    // check if file was opened successfully
    if (!this->data) {
        this->data.close();
        return false;
    }

    // return true on success
    return true;
}

void SDWrapper::closeFile() {
    this->data.close();
}
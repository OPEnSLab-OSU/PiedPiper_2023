#include "Peripherals"

SDWrapper::SD(uint8_t PIN_CS) {
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

bool SDWrapper::openFile(char *fname, uint8_t mode) {
    if (mode == FILE_READ && !SD.exists(fname)) return false;

    // open file in mode
    this->data = SD.open(fname, mode);

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
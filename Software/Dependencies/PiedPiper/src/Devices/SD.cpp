#include "Peripherals"

SD::SD(uint8_t PIN_CS) {
    this->PIN_CS = PIN_CS;
}

bool SD::initialize(void) {
    return true;
}

bool SD::begin(void) {

}

void SD::end(void) {

}

bool SD::openFile() {

}

void SD::closeFile() {

}
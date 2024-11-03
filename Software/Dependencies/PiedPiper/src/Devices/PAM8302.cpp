#include "Peripherals.h"

PAM8302::PAM8302(uint8_t PIN_SD) {
    this->PIN_SD = PIN_SD;
}

void PAM8302::powerOn() {
    digitalWrite(this->PIN_SD, HIGH);
}

void PAM8302::powerOff() {
    digitalWrite(this->PIN_SD, LOW);
}
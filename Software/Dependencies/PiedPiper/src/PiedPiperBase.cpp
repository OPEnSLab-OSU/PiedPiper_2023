#include "PiedPiper.h"

void PiedPiperBase::Hypnos_3VR_ON() {
    digitalWrite(PIN_HYPNOS_3VR, LOW);
}

void PiedPiperBase::Hypnos_3VR_OFF() {
    digitalWrite(PIN_HYPNOS_3VR, HIGH);
}

void PiedPiperBase::Hypnos_5VR_ON() {
    digitalWrite(PIN_HYPNOS_5VR, HIGH);
}

void PiedPiperBase::Hypnos_5VR_OFF() {
    digitalWrite(PIN_HYPNOS_5VR, LOW);
}

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
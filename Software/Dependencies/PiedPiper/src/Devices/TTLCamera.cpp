#include "Peripherals.h"
#include <Arduino.h>

Adafruit_VC0706 _cam = Adafruit_VC0706(&Serial1);

TTLCamera::TTLCamera() {
    this->cam = nullptr;
}

bool TTLCamera::initialize() {
    this->cam = &_cam;
    
    if (cam->begin()) {
        cam->reset();
        return true;
    }
    return false;
}

bool TTLCamera::takePhoto(File *file) {
    if (!(*file)) return false;
    
    if (!cam->begin()) return false;

    if (!cam->setImageSize(VC0706_640x480)) {
        Serial.print("Set image size failed; image size: ");
        Serial.println(cam->getImageSize());
        Serial.println(VC0706_640x480); // Should output the expected value

    }

    if (!cam->takePicture()) return false;
    
    uint32_t frameLength = cam->frameLength();

    uint8_t *buffer;
    uint8_t bytesToRead = 64;

    while (frameLength > 0) {
        bytesToRead = min(64, frameLength);
        buffer = cam->readPicture(bytesToRead);
        file->write(buffer, bytesToRead);

        frameLength -= bytesToRead;
    }
    cam->reset();

    return true;
}

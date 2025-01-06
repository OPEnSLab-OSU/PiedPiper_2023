#include "Peripherals.h"

Adafruit_VC0706 _cam = Adafruit_VC0706(&Serial1);

TTLCamera::TTLCamera() {
    
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
    if (!file) return false;

    if (!cam->begin()) return false;

    if (!cam->setImageSize(VC0706_640x480)) return false;

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
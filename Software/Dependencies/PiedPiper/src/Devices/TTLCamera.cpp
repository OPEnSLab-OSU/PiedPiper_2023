#include "Peripherals.h"

TTLCamera::TTLCamera() {
    
}

bool TTLCamera::initialize() {
    if (cam.begin()) {
        cam.reset();
        return true;
    }
    return false;
}

bool TTLCamera::cameraOn() {
    return cam.begin();
}

bool TTLCamera::cameraOff() {
    return cam.reset();
}

bool TTLCamera::takePhoto(File *file) {
    if (!file) return false;

    if (!cam.begin()) return false;

    if (!cam.setImageSize(VC0706_640x480)) return false;

    if (!cam.takePicture()) return false;

    uint32_t frameLength = cam.frameLength();

    uint8_t *buffer;
    uint8_t bytesToRead = 64;

    while (frameLength > 0) {
        bytesToRead = min(64, frameLength);
        buffer = cam.readPicture(bytesToRead);
        file->write(buffer, bytesToRead);

        frameLength -= bytesToRead;
    }

    cam.reset();

    return true;
}
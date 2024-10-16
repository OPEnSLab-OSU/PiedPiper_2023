#include "Peripherals.h"

RTC::RTC(uint8_t i2c_address) {
    this->i2c_address = i2c_address;
}

bool RTC::initialize() {
    return true;
}

DateTime RTC::getDateTime() {
    return NULL;
}

bool RTC::clearAlarm() {

}

bool RTC::setAlarm() {

}

bool RTC::setAlarmSeconds() {

}
#include "Peripherals.h"

RTC::RTC(uint8_t i2c_address) {
    this->i2c_address = i2c_address;
}

bool RTC::initialize() {
    if (!this->rtc.begin()) return false;

    if (!this->rtc.lostPower()) {
        Serial.println("RTC lost power, check voltage on HYPNOS battery. RTC should be properly adjusted!");
        this->rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    // reset rtc alarms
    this->rtc.clearAlarm(1);
    this->rtc.clearAlarm(2);

    this->rtc.writeSqwPinMode(DS3231_OFF);

    this->rtc.disableAlarm(2);

    return true;
}

DateTime RTC::getDateTime() {
    return this->rtc.now();
}

bool RTC::clearAlarm() {
    
}

bool RTC::setAlarm() {

}

bool RTC::setAlarmSeconds() {

}
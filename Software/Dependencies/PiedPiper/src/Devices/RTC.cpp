#include "Peripherals.h"

RTCWrapper::RTCWrapper(uint8_t i2c_address) {
    this->i2c_address = i2c_address;
}

bool RTCWrapper::initialize() {
    if (!this->rtc.begin()) return false;

    if (!this->rtc.lostPower()) {
        Serial.println("RTC lost power, check voltage on HYPNOS battery. RTC should be properly adjusted!");
        this->rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    // reset rtc alarms
    this->rtc.clearAlarm(1);
    this->rtc.clearAlarm(2);

    // configure to use alarm
    this->rtc.writeSqwPinMode(DS3231_OFF);

    // disable alarm 2 since we aren't using it
    this->rtc.disableAlarm(2);

    return true;
}

DateTime RTCWrapper::getDateTime() {
    return this->rtc.now();
}

bool RTCWrapper::clearAlarm() {
    rtc.clearAlarm(1);
}

bool RTCWrapper::setAlarm(DateTime alarmDateTime) {
    if (rtc.setAlarm1(
        alarmDateTime,
        DS3231_A1_Hour // this mode triggers the alarm when the hours, minutes and seconds match
    )) {
        return true;
    }

    return false;
}

bool RTCWrapper::setAlarmSeconds(int32_t seconds) {
    DateTime nowDT = this->getDateTime();

    if (rtc.setAlarm1(
        // TimeSpan() accepts seconds as argument. Subtracting the current seconds isn't necassary, but helps to ensure that alarm goes of exactly at the minute mark 
        nowDT + TimeSpan(seconds - nowDT.second()),
        DS3231_A1_Hour // this mode triggers the alarm when the hours, minutes and seconds match
    )) {
        return true;
    }

    return false;
}
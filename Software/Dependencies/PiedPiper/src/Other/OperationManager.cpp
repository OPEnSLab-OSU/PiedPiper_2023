#include "OperationManager.h"

OperationManager::OperationManager() {
    this->operationTimes = NULL;
    this->numOperationTimes = 0;
}

void OperationManager::addOperationTime(uint8_t hour, uint8_t minute) {
    // increment size of operatimesTimes array
    this->numOperationTimes += 1;
    // reallocate memory for new array size
    this->operationTimes = (opTime*)realloc(this->operationTimes, this->numOperationTimes * sizeof(opTime));
    // store data
    this->operationTimes[this->numOperationTimes - 1].hour = hour;
    this->operationTimes[this->numOperationTimes - 1].minute = minute;
    this->operationTimes[this->numOperationTimes - 1].minutes = hour * 60 + minute;
}

uint16_t OperationManager::minutesTillNextOperationTime(DateTime dateTime, bool &isWithinOperationInterval) {
    // convert the current time to minutes (0 - 1440)
    int nowMinutes = dateTime.hour() * 60 + dateTime.minute();
    int nowSecond = dateTime.second();

    // stores the difference between current time and next alarm time for setting RTC alarm
    int nextAlarmTime = -1;

    // final return value, indicating whether or not playback should be performed
    bool performPlayback = 0;

    // Device will trigger alarm once every 1440 minutes if no operation times are defined
    if (this->numOperationTimes == 0) nextAlarmTime = 1440;
    // Otherwise, alarm will be set based on the current operation interval
    else {
        // check if time is within time of operation interval
        for (int i = 0; i < this->numOperationTimes - 1; i += 2) {
            //Serial.printf("Now: %d, Start: %d, End: %d\n", nowMinutes, operationTimes[i].minutes, operationTimes[i + 1].minutes);
            if (nowMinutes >= this->operationTimes[i].minutes && nowMinutes < this->operationTimes[i + 1].minutes) {
                // if within an operation interval, then playback will be performed
                performPlayback = 1;
                // the next alarm time is the difference between the end of this operation time and the current time (in minutes)
                nextAlarmTime = this->operationTimes[i + 1].minutes - nowMinutes;
                break;
            }
        }

        // if the current time is not within an operation time then set nextAlarmTime to the next available operation time
        if (!performPlayback) {
            for (int i = 0; i < this->numOperationTimes - 1; i += 2) {
                // the next available operation time is determined when the start of an operation interval exceeds the current time (in minutes)
                if (this->operationTimes[i].minutes > nowMinutes) {
                    // the next alarm time is the difference between the start of the next operation interval and the current time
                    nextAlarmTime = this->operationTimes[i].minutes - nowMinutes;
                    break;
                }
            }
        }

        // if nextAlarmTime wasn't set (if no operation times exist after the current time), then set to first operation time (24 hour wrap)
        if (nextAlarmTime == -1) {
            // next alarm time is the difference between the current time and the first available operation time
            nextAlarmTime = 1440 - nowMinutes + this->operationTimes[0].minutes;
        }
    }

    isWithinOperationInterval = performPlayback;
    return nextAlarmTime;
}

void OperationManager::printOperationTimes(void) {
    Serial.println("Printing operation times...");
    for (int i = 0; i < numOperationTimes; i = i + 2) {
        Serial.printf("\t%02d:%02d - %02d:%02d\n", this->operationTimes[i].hour, this->operationTimes[i].minute, this->operationTimes[i + 1].hour, this->operationTimes[i + 1].minute);
    }
    Serial.println();
}
#ifndef OPERATION_MANAGER_h
#define OPERATION_MANAGER_h

#include "RTClib.h"

struct opTime {
    uint8_t hour;   ///< the corresponding hour
    uint8_t minute; ///< the corresponding minute
    int16_t minutes;  ///< total minutes (hour * 60 + minute)
};

class OperationManager
{

    private:

        opTime *operationTimes;

        uint16_t numOperationTimes;

    public:

        OperationManager();

        // adds an operation time to operation manager
        void addOperationTime(uint8_t hour, uint8_t minute);

        // returns minutes until the next operation time given a date time
        uint16_t minutesTillNextOperationTime(DateTime dateTime, bool &isWithinOperationInterval);

        void printOperationTimes(void);

};


#endif
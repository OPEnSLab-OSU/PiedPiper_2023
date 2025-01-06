#ifndef OPERATION_MANAGER_h
#define OPERATION_MANAGER_h

#include "RTClib.h"

/**
 * struct for storing operation details
 */
struct opTime {
    uint8_t hour;       ///< the corresponding hour
    uint8_t minute;     ///< the corresponding minute
    int16_t minutes;    ///< total minutes (hour * 60 + minute)
};

/**
 * class for managing device operations such as when device should be active or asleep
 */
class OperationManager
{

    private:

        opTime *operationTimes;         ///< pointer to dynamically allocated array

        uint16_t numOperationTimes;     ///< number of operation times in operationTimes array

    public:

        /**
         * constructor for OperationManager
         */
        OperationManager();

        /**
         * adds an operation time to operation manager
         * @param hour hour
         * @param minute minute
         */
        void addOperationTime(uint8_t hour, uint8_t minute);

        /**
         * returns minutes until the next operation time given a date time
         * @param dateTime DateTime to use as reference
         * @param isWithinOperationInterval reference to boolean indicating whether passed date time is within operation interval
         * @return minutes until next operation interval
         */
        uint16_t minutesTillNextOperationTime(DateTime dateTime, bool &isWithinOperationInterval);

        /**
         * prints operation intervals
         */
        void printOperationTimes(void);

};


#endif
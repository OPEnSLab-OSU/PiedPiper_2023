#include "Peripherals.h"
#include "SAMDTimerInterrupt.h"

SAMDTimer SAMPLING_TIMER(TIMER_TC3);

TimerInterruptController::TimerInterruptController() {

}

void TimerInterruptController::initialize() {
    SAMPLING_TIMER.attachInterruptInterval(1000, blankFunction);
    detachTimerInterrupt();
}

void TimerInterruptController::attachTimerInterrupt(const unsigned long interval_us, void(*functionPtr)()) {
    SAMPLING_TIMER.attachInterruptInterval(interval_us, functionPtr);
    SAMPLING_TIMER.restartTimer();
}

void TimerInterruptController::detachTimerInterrupt() {
    SAMPLING_TIMER.detachInterrupt();
}

void TimerInterruptController::blankFunction() {
    return;
}
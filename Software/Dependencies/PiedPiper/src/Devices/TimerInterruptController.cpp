#include "Peripherals.h"
#include "SAMDTimerInterrupt.h"

SAMDTimer TimerInterruptController::SAMPLING_TIMER(TIMER_TC3);

void TimerInterruptController::initialize() {
    this->SAMPLING_TIMER.attachInterruptInterval(sampleDelayTime, this->blankFunction);   // enable interrupt
    this->SAMPLING_TIMER.detachInterrupt();    
    this->SAMPLING_TIMER.stopTimer();
}

void TimerInterruptController::attachTimerInterrupt(const unsigned long interval_us, void(*functionPtr)()) {
    this->SAMPLING_TIMER.attachInterruptInterval(interval_us, functionPtr);
    this->SAMPLING_TIMER.restartTimer();
}

void TimerInterruptController::detachTimerInterrupt() {
    this->SAMPLING_TIMER.detachInterrupt();    
    this->SAMPLING_TIMER.restartTimer();
}

void TimerInterruptController::blankFunction() {
    return;
}
#include "Peripherals.h"

SleepController::SleepController() {

}

void SleepController::goToSleep(SLEEPMODES mode) {
    // end USB Serial, probably can be commented out
    USBDevice.detach();
    USBDevice.end();

    // ensure that the correct sleep mode is set
    PM->SLEEPCFG.bit.SLEEPMODE = mode;
    while(PM->SLEEPCFG.bit.SLEEPMODE != mode);

    // go to sleep by ensuring all memory accesses are complete with (__DSB) and calling wait for interrupt (__WFI)
    __DSB();
    __WFI();
}
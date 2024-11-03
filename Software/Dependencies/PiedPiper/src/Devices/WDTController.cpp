#include "Peripherals.h"

WDTController::WDTController() {
    
}

void WDTController::start() {
    REG_WDT_CONFIG = WDT_CONFIG_PER_CYC4096;
    REG_WDT_CTRLA = WDT_CTRLA_ENABLE;
}

void WDTController::reset() {
    REG_WDT_CLEAR = WDT_CLEAR_CLEAR_KEY;
}
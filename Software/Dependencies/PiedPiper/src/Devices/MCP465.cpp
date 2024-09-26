#include "Peripherals.h"

#define MCP465_INC_WIPER_CMD 0b00000100
#define MCP465_DEC_WIPER_CMD 0b00001000
#define MCP465_READ_WIPER_CMD 0b00001100
#define MCP465_WRITE_WIPER_CMD 0b00000000

MCP465::MCP465(uint8_t i2c_address) {
    this->i2c_address = i2c_address;
}

uint8_t MCP465::incrementWiper() {
    Wire.beginTransmission(this->i2c_address);
    Wire.write(MCP465_INC_WIPER_CMD);
    return Wire.endTransmission();
}

uint8_t MCP465::decrementWiper() {
    Wire.beginTransmission(this->i2c_address);
    Wire.write(MCP465_DEC_WIPER_CMD);
    return Wire.endTransmission();
}

uint8_t MCP465::writeWiperValue(uint8_t aValue) {
    Wire.beginTransmission(this->i2c_address);
    Wire.write(MCP465_WRITE_WIPER_CMD);
    Wire.write(aValue);
    return Wire.endTransmission();
}

int16_t MCP465::readWiperValue() {
    Wire.beginTransmission(this->i2c_address);
    Wire.write(MCP465_READ_WIPER_CMD);
    if (Wire.endTransmission() > 0) return -1;
    Wire.requestFrom(MCP465_ADDR, 2);
    uint16_t bitsHigh = 0;
    bitsHigh = Wire.read() << 8;
    return bitsHigh | Wire.read();
}
#include "Peripherals.h"

#define MCP465_INC_WIPER_CMD 0x4
#define MCP465_DEC_WIPER_CMD 0x8
#define MCP465_READ_WIPER_CMD 0xC
#define MCP465_WRITE_WIPER_CMD 0x0

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
    Wire.requestFrom(this->i2c_address, 2);
    uint16_t _bitsHigh = 0;
    _bitsHigh = Wire.read() << 8;
    return _bitsHigh | Wire.read();
}
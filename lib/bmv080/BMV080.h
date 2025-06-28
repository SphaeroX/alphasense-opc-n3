#ifndef BMV080_H
#define BMV080_H

#include <Wire.h>

class BMV080
{
public:
    static constexpr uint8_t I2C_ADDRESS_BASE = 0x54;
    static constexpr uint16_t CMD_RESET = 0xD000;
    static constexpr uint16_t CMD_START_CONTINUOUS = 0xD100;
    static constexpr uint16_t CMD_STOP_MEASUREMENT = 0xD200;
    static constexpr uint16_t CMD_READ_OUTPUT = 0xE000;

    BMV080(uint8_t address = I2C_ADDRESS_BASE);
    bool begin(TwoWire &wire = Wire);
    bool reset();
    bool startContinuous(uint8_t algorithm = 3);
    bool stopMeasurement();
    bool readOutput(float &pm1, float &pm2_5, float &pm10, bool &obstruction, bool &outOfRange);

private:
    TwoWire *_wire;
    uint8_t _addr;
    bool writeCmd(uint16_t header, const uint16_t *payload, size_t len);
    bool readCmd(uint16_t header, uint16_t *payload, size_t len);
};

#endif // BMV080_H

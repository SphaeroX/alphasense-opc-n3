#include "BMV080.h"

BMV080::BMV080(uint8_t address)
    : _wire(nullptr), _addr(address) {}

bool BMV080::begin(TwoWire &wire)
{
    _wire = &wire;
    _wire->begin();
    return true;
}

bool BMV080::reset()
{
    return writeCmd(CMD_RESET, nullptr, 0);
}

bool BMV080::startContinuous(uint8_t algorithm)
{
    uint16_t alg = static_cast<uint16_t>(algorithm);
    return writeCmd(CMD_START_CONTINUOUS, &alg, 1);
}

bool BMV080::stopMeasurement()
{
    return writeCmd(CMD_STOP_MEASUREMENT, nullptr, 0);
}

bool BMV080::readOutput(float &pm1, float &pm2_5, float &pm10, bool &obstruction, bool &outOfRange)
{
    uint16_t data[5];
    if (!readCmd(CMD_READ_OUTPUT, data, 5))
        return false;
    pm1 = static_cast<float>(data[0]);
    pm2_5 = static_cast<float>(data[1]);
    pm10 = static_cast<float>(data[2]);
    obstruction = (data[3] & 0x0001) != 0;
    outOfRange = (data[3] & 0x0002) != 0;
    return true;
}

bool BMV080::writeCmd(uint16_t header, const uint16_t *payload, size_t len)
{
    _wire->beginTransmission(_addr);
    _wire->write(static_cast<uint8_t>(header >> 8));
    _wire->write(static_cast<uint8_t>(header & 0xFF));
    for (size_t i = 0; i < len; ++i)
    {
        _wire->write(static_cast<uint8_t>(payload[i] >> 8));
        _wire->write(static_cast<uint8_t>(payload[i] & 0xFF));
    }
    return (_wire->endTransmission() == 0);
}

bool BMV080::readCmd(uint16_t header, uint16_t *payload, size_t len)
{
    if (!writeCmd(header, nullptr, 0))
        return false;
    const size_t bytes = len * 2;
    if (_wire->requestFrom(static_cast<int>(_addr), static_cast<int>(bytes)) != static_cast<int>(bytes))
    {
        return false;
    }
    for (size_t i = 0; i < len; ++i)
    {
        uint16_t msb = _wire->read();
        uint16_t lsb = _wire->read();
        payload[i] = (msb << 8) | lsb;
    }
    return true;
}

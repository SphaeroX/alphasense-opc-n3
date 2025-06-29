#include "Bmv080.h"

bool Bmv080::begin() {
    // Stub initialization
    return true;
}

bool Bmv080::readData(Bmv080Data &data) {
    // Return dummy data for now
    data.temperature_c = 25.0f;
    data.humidity_rh = 50.0f;
    data.pressure_pa = 101325.0f;
    return true;
}

#ifndef BMV080_H
#define BMV080_H
#include <Arduino.h>
struct Bmv080Data {
    float temperature_c;
    float humidity_rh;
    float pressure_pa;
};
class Bmv080 {
public:
    bool begin();
    bool readData(Bmv080Data &data);
};
#endif

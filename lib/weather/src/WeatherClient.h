#ifndef WEATHER_CLIENT_H
#define WEATHER_CLIENT_H

#include <Arduino.h>

struct WeatherData {
    float temperature_c;
    float humidity_rh;
    float precipitation_mm;
    float cloud_cover_pct;
    float wind_speed_kmh;
    float wind_direction_deg;
    float pollen_grass;
    float pollen_tree;
    float pollen_weed;
    bool valid;
};

class WeatherClient {
public:
    WeatherClient(float latitude, float longitude);
    bool update();
    const WeatherData &data() const { return _data; }

private:
    float _latitude;
    float _longitude;
    WeatherData _data;
    bool fetchCurrent();
    bool fetchPollen();
};

#endif // WEATHER_CLIENT_H

#ifndef WEATHER_CLIENT_H
#define WEATHER_CLIENT_H

#include <Arduino.h>

struct WeatherData {
    float temperature_c;
    float humidity_rh;
    float apparent_temperature_c;
    bool is_day;
    float rain_mm;
    float cloud_cover_pct;
    float pressure_msl_hpa;
    float surface_pressure_hpa;
    float wind_speed_kmh;
    float wind_direction_deg;
    float wind_gusts_kmh;
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
};

#endif // WEATHER_CLIENT_H

#ifndef OPEN_METEO_CLIENT_H
#define OPEN_METEO_CLIENT_H

#include <Arduino.h>

struct OpenMeteoData {
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
    // Air quality fields
    float ragweed_pollen_grains_m3;
    float olive_pollen_grains_m3;
    float mugwort_pollen_grains_m3;
    float grass_pollen_grains_m3;
    float birch_pollen_grains_m3;
    float alder_pollen_grains_m3;
    float dust_ug_m3;
    float carbon_monoxide_ug_m3;
    float pm2_5_ug_m3;
    float pm10_ug_m3;
    float european_aqi;
    bool valid;
};

class OpenMeteoClient {
public:
    OpenMeteoClient(float latitude, float longitude);
    bool update();
    const OpenMeteoData &data() const { return _data; }

private:
    float _latitude;
    float _longitude;
    OpenMeteoData _data;
    bool fetchCurrent();
    bool fetchAirQuality();
};

#endif // OPEN_METEO_CLIENT_H

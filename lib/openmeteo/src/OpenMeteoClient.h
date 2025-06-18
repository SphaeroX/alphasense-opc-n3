#ifndef OPEN_METEO_CLIENT_H
#define OPEN_METEO_CLIENT_H

#include <Arduino.h>
#include <ArduinoJson.h>

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
    explicit OpenMeteoClient(float latitude, float longitude,
                             uint32_t intervalMs = 600000);
    bool update();
    const OpenMeteoData &data() const { return _data; }

private:
    static constexpr uint32_t HTTP_TIMEOUT_MS = 10000; // 10s timeout
    static constexpr uint8_t MAX_RETRIES = 3;
    float _latitude;
    float _longitude;
    uint32_t _minUpdateInterval;
    unsigned long _lastUpdateMs;
    OpenMeteoData _data;
    bool fetchCurrent();
    bool fetchAirQuality();
    bool fetchJson(const String &url, JsonDocument &doc);
};

#endif // OPEN_METEO_CLIENT_H

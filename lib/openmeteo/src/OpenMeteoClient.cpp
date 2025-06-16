#include "OpenMeteoClient.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>

OpenMeteoClient::OpenMeteoClient(float latitude, float longitude)
    : _latitude(latitude), _longitude(longitude)
{
    memset(&_data, 0, sizeof(_data));
    _data.valid = false;
}

bool OpenMeteoClient::update()
{
    bool weatherOk = fetchCurrent();
    bool airOk = fetchAirQuality();
    _data.valid = weatherOk || airOk;
    return _data.valid;
}

bool OpenMeteoClient::fetchCurrent()
{
    HTTPClient http;
    String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + String(_latitude, 4) +
                 "&longitude=" + String(_longitude, 4) +
                 "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,rain,cloud_cover,pressure_msl,surface_pressure,wind_speed_10m,wind_direction_10m,wind_gusts_10m&timezone=Europe%2FBerlin";
    Serial.print("Fetching weather from: ");
    Serial.println(url);
    http.begin(url);
    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        http.end();
        return false;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    http.end();
    if (err)
    {
        return false;
    }
    JsonObject current = doc["current"];
    _data.temperature_c = current["temperature_2m"].as<float>();
    _data.humidity_rh = current["relative_humidity_2m"].as<float>();
    _data.apparent_temperature_c = current["apparent_temperature"].as<float>();
    _data.is_day = current["is_day"].as<int>() == 1;
    _data.rain_mm = current["rain"].as<float>();
    _data.cloud_cover_pct = current["cloud_cover"].as<float>();
    _data.pressure_msl_hpa = current["pressure_msl"].as<float>();
    _data.surface_pressure_hpa = current["surface_pressure"].as<float>();
    _data.wind_speed_kmh = current["wind_speed_10m"].as<float>();
    _data.wind_direction_deg = current["wind_direction_10m"].as<float>();
    _data.wind_gusts_kmh = current["wind_gusts_10m"].as<float>();
    return true;
}

bool OpenMeteoClient::fetchAirQuality()
{
    HTTPClient http;
    String url = String("https://air-quality-api.open-meteo.com/v1/air-quality?latitude=") + String(_latitude, 4) +
                 "&longitude=" + String(_longitude, 4) +
                 "&current=ragweed_pollen,olive_pollen,mugwort_pollen,grass_pollen,birch_pollen,alder_pollen,dust,carbon_monoxide,pm2_5,pm10,european_aqi&timezone=Europe%2FBerlin";
    Serial.print("Fetching air quality from: ");
    Serial.println(url);
    http.begin(url);
    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        http.end();
        return false;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    http.end();
    if (err)
    {
        return false;
    }
    JsonObject current = doc["current"];
    _data.ragweed_pollen_grains_m3 = current["ragweed_pollen"].as<float>();
    _data.olive_pollen_grains_m3 = current["olive_pollen"].as<float>();
    _data.mugwort_pollen_grains_m3 = current["mugwort_pollen"].as<float>();
    _data.grass_pollen_grains_m3 = current["grass_pollen"].as<float>();
    _data.birch_pollen_grains_m3 = current["birch_pollen"].as<float>();
    _data.alder_pollen_grains_m3 = current["alder_pollen"].as<float>();
    _data.dust_ug_m3 = current["dust"].as<float>();
    _data.carbon_monoxide_ug_m3 = current["carbon_monoxide"].as<float>();
    _data.pm2_5_ug_m3 = current["pm2_5"].as<float>();
    _data.pm10_ug_m3 = current["pm10"].as<float>();
    _data.european_aqi = current["european_aqi"].as<float>();
    return true;
}


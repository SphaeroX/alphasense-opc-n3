#include "WeatherClient.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

WeatherClient::WeatherClient(float latitude, float longitude)
    : _latitude(latitude), _longitude(longitude)
{
    memset(&_data, 0, sizeof(_data));
    _data.valid = false;
}

bool WeatherClient::update()
{
    _data.valid = fetchCurrent();
    return _data.valid;
}

bool WeatherClient::fetchCurrent()
{
    HTTPClient http;
    String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + String(_latitude, 4) +
                 "&longitude=" + String(_longitude, 4) +
                 "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,rain,cloud_cover,pressure_msl,surface_pressure,wind_speed_10m,wind_direction_10m,wind_gusts_10m&timezone=Europe%2FBerlin";
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


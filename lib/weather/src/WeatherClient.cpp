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
    bool ok1 = fetchCurrent();
    bool ok2 = fetchPollen();
    _data.valid = ok1 && ok2;
    return _data.valid;
}

bool WeatherClient::fetchCurrent()
{
    HTTPClient http;
    String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + String(_latitude, 4) +
                 "&longitude=" + String(_longitude, 4) +
                 "&current=temperature_2m,relative_humidity_2m,precipitation,cloudcover,wind_speed_10m,wind_direction_10m&timezone=UTC";
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
    _data.precipitation_mm = current["precipitation"].as<float>();
    _data.cloud_cover_pct = current["cloudcover"].as<float>();
    _data.wind_speed_kmh = current["wind_speed_10m"].as<float>();
    _data.wind_direction_deg = current["wind_direction_10m"].as<float>();
    return true;
}

bool WeatherClient::fetchPollen()
{
    HTTPClient http;
    String url = String("https://air-quality-api.open-meteo.com/v1/air-quality?latitude=") + String(_latitude, 4) +
                 "&longitude=" + String(_longitude, 4) +
                 "&hourly=grass_pollen,tree_pollen,weed_pollen&forecast_days=1&timezone=UTC";
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
    JsonObject hourly = doc["hourly"];
    if (!hourly["time"].is<JsonArray>())
    {
        return false;
    }
    _data.pollen_grass = hourly["grass_pollen"][0].as<float>();
    _data.pollen_tree = hourly["tree_pollen"][0].as<float>();
    _data.pollen_weed = hourly["weed_pollen"][0].as<float>();
    return true;
}

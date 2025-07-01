#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include "SparkFun_BMV080_Arduino_Library.h"
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "config.h"
#include <time.h>

const unsigned long measurementSleepMs = SENSOR_SLEEP_MS;

SparkFunBMV080 bmv080;

// Attempt to reconnect the BMV080 sensor over I2C
static bool reconnectBmv080()
{
    Serial.println("Attempting BMV080 reconnect...");

    // Close current connection if possible
    bmv080.close();
    Wire.end();
    delay(10); // short pause before reinitializing

    // Reinitialize I2C and sensor
    Wire.begin();
    if (!bmv080.begin(SF_BMV080_DEFAULT_ADDRESS, Wire))
    {
        Serial.println("BMV080 begin() failed during reconnect");
        return false;
    }

    if (!bmv080.init())
    {
        Serial.println("BMV080 init() failed during reconnect");
        return false;
    }

    if (!bmv080.setMode(SF_BMV080_MODE_CONTINUOUS))
    {
        Serial.println("BMV080 setMode() failed during reconnect");
        return false;
    }

    Serial.println("BMV080 reconnected successfully");
    return true;
}

#if defined(ESP32)
#define DEVICE "ESP32"
#else
#define DEVICE "ARDUINO"
#endif

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point sensorPoint("bmv080");

static void waitForTimeSync()
{
    time_t nowSecs = time(nullptr);
    Serial.print("Waiting for time sync");
    while (nowSecs < 1609459200)
    {
        Serial.print(".");
        delay(500);
        nowSecs = time(nullptr);
    }
    Serial.println(" done");
}

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ;
    Serial.println("\n\nBMV080 Reader");

    Serial.printf("Connecting to WiFi '%s'", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }
    Serial.println(" connected");

    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
    waitForTimeSync();

    client.setWriteOptions(WriteOptions().writePrecision(WritePrecision::S));
    sensorPoint.addTag("device", DEVICE);
    sensorPoint.addTag("ssid", WiFi.SSID());

    if (client.validateConnection())
    {
        Serial.print("Connected to InfluxDB: ");
        Serial.println(client.getServerUrl());
    }
    else
    {
        Serial.print("InfluxDB connection failed: ");
        Serial.println(client.getLastErrorMessage());
    }

    Wire.begin();
    if (!bmv080.begin(SF_BMV080_DEFAULT_ADDRESS, Wire))
    {
        Serial.println("BMV080 not detected. Check wiring.");
        while (1)
            ;
    }
    Serial.println("BMV080 found!");

    bmv080.init();
    if (bmv080.setMode(SF_BMV080_MODE_CONTINUOUS))
    {
        Serial.println("BMV080 set to continuous mode");
    }
    else
    {
        Serial.println("Error setting BMV080 mode");
    }
}

void loop()
{
    static unsigned long lastMeasurementMs = 0;
    unsigned long now = millis();
    if (now - lastMeasurementMs < measurementSleepMs)
    {
        return;
    }
    lastMeasurementMs = now;

    bool success = false;
    for (int i = 0; i < 20; ++i)
    {
        if (bmv080.readSensor())
        {
            success = true;
            break;
        }
        delay(100); // allow sensor to update
    }

    if (success)
    {
        float pm1 = bmv080.PM1();
        float pm25 = bmv080.PM25();
        float pm10 = bmv080.PM10();
        bool obstructed = bmv080.isObstructed();

        Serial.printf("PM1: %.2f \tPM2.5: %.2f \tPM10: %.2f", pm1, pm25, pm10);
        if (obstructed)
        {
            Serial.print("\tObstructed");
        }
        Serial.println();

        sensorPoint.clearFields();
        sensorPoint.addField("bmv_pm1", pm1);
        sensorPoint.addField("bmv_pm2_5", pm25);
        sensorPoint.addField("bmv_pm10", pm10);
        sensorPoint.addField("bmv_obstructed", obstructed ? 1 : 0);
        sensorPoint.setTime();

        Serial.print("Writing to InfluxDB: ");
        Serial.println(client.pointToLineProtocol(sensorPoint));
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("WiFi connection lost");
        }
        if (!client.writePoint(sensorPoint))
        {
            Serial.print("InfluxDB write failed: ");
            Serial.println(client.getLastErrorMessage());
        }
    }
    else
    {
        Serial.println("Error reading BMV080 measurement");
        reconnectBmv080();
    }
}


#include <Arduino.h>
#include <WiFi.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "config.h"
#include <time.h>
#include "Bmv080.h"

const unsigned long measurementSleepMs = SENSOR_SLEEP_MS;

Bmv080 bmv;

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

    if (!bmv.begin()) {
        Serial.println("Failed to initialize BMV080 sensor");
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

    Bmv080Data data;
    if (bmv.readData(data))
    {
        Serial.printf("Temperature: %.2f C\n", data.temperature_c);
        Serial.printf("Humidity: %.2f %%RH\n", data.humidity_rh);
        Serial.printf("Pressure: %.2f Pa\n", data.pressure_pa);

        sensorPoint.clearFields();
        sensorPoint.addField("bmv080_temperature", data.temperature_c);
        sensorPoint.addField("bmv080_humidity", data.humidity_rh);
        sensorPoint.addField("bmv080_pressure", data.pressure_pa);
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
    }
}


#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "config.h"
#include <time.h>

const unsigned long measurementSleepMs = SENSOR_SLEEP_MS;

SensirionI2cScd4x scd4x;

#if defined(ESP32)
#define DEVICE "ESP32"
#else
#define DEVICE "ARDUINO"
#endif

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point sensorPoint("scd41");

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
    Serial.println("\n\nSCD41 CO2 Reader");

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
    scd4x.begin(Wire, SCD41_I2C_ADDR_62);
    scd4x.wakeUp();
    scd4x.stopPeriodicMeasurement();
    scd4x.reinit();
    scd4x.startPeriodicMeasurement();
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

    bool scdReady = false;
    uint16_t co2 = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;

    int16_t err = scd4x.getDataReadyStatus(scdReady);
    if (err == 0 && scdReady)
    {
        err = scd4x.readMeasurement(co2, temperature, humidity);
        if (err == 0)
        {
            Serial.printf("CO2: %u ppm\n", co2);
            Serial.printf("Temperature: %.2f C\n", temperature);
            Serial.printf("Humidity: %.2f %%RH\n", humidity);

            sensorPoint.clearFields();
            sensorPoint.addField("scd41_co2", co2);
            sensorPoint.addField("scd41_temperature", temperature);
            sensorPoint.addField("scd41_humidity", humidity);
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
            Serial.println("Error reading SCD41 measurement");
        }
    }
    else if (err != 0)
    {
        Serial.println("Error checking SCD41 data ready status");
    }
}


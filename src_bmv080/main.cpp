#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include "SparkFun_BMV080_Arduino_Library.h"
#include "OpcN3.h"
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "config.h"
#include <time.h>

const unsigned long measurementSleepMs = SENSOR_SLEEP_MS;

// OPC-N3 SPI pin configuration
const int OPC_MOSI_PIN = 23;
const int OPC_MISO_PIN = 19;
const int OPC_SCK_PIN  = 18;
const int OPC_SS_PIN   = 5;

SparkFunBMV080 bmv080;
OpcN3 opc(OPC_SS_PIN);

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
    Serial.println("\n\nBMV080 + OPC Reader");

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

    // Initialize buses
    SPI.begin(OPC_SCK_PIN, OPC_MISO_PIN, OPC_MOSI_PIN, OPC_SS_PIN);
    Wire.begin();

    // Initialize BMV080
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

    // Initialize OPC-N3
    if (!opc.begin())
    {
        Serial.println("FATAL: OPC-N3 initialization failed. Program halted.");
        while (1)
            ;
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

    bool bmvSuccess = false;
    for (int i = 0; i < 20; ++i)
    {
        if (bmv080.readSensor())
        {
            bmvSuccess = true;
            break;
        }
        delay(100); // allow sensor to update
    }

    float bmvPm1 = NAN, bmvPm25 = NAN, bmvPm10 = NAN;
    bool bmvObstructed = false;
    if (bmvSuccess)
    {
        bmvPm1 = bmv080.PM1();
        bmvPm25 = bmv080.PM25();
        bmvPm10 = bmv080.PM10();
        bmvObstructed = bmv080.isObstructed();
        Serial.printf("BMV080 PM1: %.2f \tPM2.5: %.2f \tPM10: %.2f", bmvPm1, bmvPm25, bmvPm10);
        if (bmvObstructed)
        {
            Serial.print("\tObstructed");
        }
        Serial.println();
    }
    else
    {
        Serial.println("Error reading BMV080 measurement");
    }

    // Read OPC-N3
    OpcN3Data opcData;
    bool opcSuccess = opc.readData(opcData);
    float opcPm1 = NAN, opcPm25 = NAN, opcPm10 = NAN;
    if (opcSuccess)
    {
        opcPm1 = opcData.pm_a;
        opcPm25 = opcData.pm_b;
        opcPm10 = opcData.pm_c;
        Serial.printf("OPC-N3 PM1: %.2f \tPM2.5: %.2f \tPM10: %.2f\n", opcPm1, opcPm25, opcPm10);
    }
    else
    {
        Serial.println("Error reading OPC-N3 measurement");
    }

    // Build InfluxDB point
    sensorPoint.clearFields();
    if (bmvSuccess)
    {
        sensorPoint.addField("bmv_pm1", bmvPm1);
        sensorPoint.addField("bmv_pm2_5", bmvPm25);
        sensorPoint.addField("bmv_pm10", bmvPm10);
        sensorPoint.addField("bmv_obstructed", bmvObstructed ? 1 : 0);
    }
    if (opcSuccess)
    {
        sensorPoint.addField("opc_pm1", opcPm1);
        sensorPoint.addField("opc_pm2_5", opcPm25);
        sensorPoint.addField("opc_pm10", opcPm10);
    }
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


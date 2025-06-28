#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "OpcN3.h"
#include "BMV080.h"
#include "config.h"
#include <time.h>

const int OPC_MOSI_PIN = 23;
const int OPC_MISO_PIN = 19;
const int OPC_SCK_PIN = 18;
const int OPC_SS_PIN = 5;

const unsigned long measurementSleepMs = SENSOR_SLEEP_MS;

OpcN3 opc(OPC_SS_PIN);
BMV080 bmv;

#if defined(ESP32)
#define DEVICE "ESP32"
#else
#define DEVICE "ARDUINO"
#endif

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET_COMPARE, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point sensorPoint("opc_bmv");

static void waitForTimeSync()
{
    time_t nowSecs = time(nullptr);
    Serial.print("Waiting for time sync");
    while (nowSecs < 1609459200) // 2021-01-01
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
    Serial.println("\n\nOPC-N3 and BMV080 Comparison");

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

    SPI.begin(OPC_SCK_PIN, OPC_MISO_PIN, OPC_MOSI_PIN, OPC_SS_PIN);
    if (!opc.begin())
    {
        Serial.println("FATAL: OPC-N3 initialization failed.");
        while (1)
            ;
    }

    Wire.begin();
    if (!bmv.begin(Wire))
    {
        Serial.println("FATAL: BMV080 initialization failed.");
        while (1)
            ;
    }
    bmv.startContinuous();
}

void loop()
{
    static bool discard_next_success = true;
    static unsigned long lastMeasurementMs = 0;
    unsigned long now = millis();
    if (now - lastMeasurementMs < measurementSleepMs)
        return;
    lastMeasurementMs = now;

    OpcN3Data opcData;
    float bmvPm1 = 0, bmvPm2_5 = 0, bmvPm10 = 0;
    bool obstruction = false, outOfRange = false;

    bool opcOk = opc.readData(opcData);
    bool bmvOk = bmv.readOutput(bmvPm1, bmvPm2_5, bmvPm10, obstruction, outOfRange);

    if (opcOk && !discard_next_success)
    {
        Serial.println("OPC-N3 data OK");
        Serial.printf("Temperature: %.2f C\n", opcData.temperature_c);
        Serial.printf("Humidity: %.2f %%RH\n", opcData.humidity_rh);
    }
    else if (opcOk)
    {
        Serial.println("First OPC-N3 reading discarded");
        discard_next_success = false;
        return;
    }
    else
    {
        Serial.println("Failed to read OPC-N3");
        return;
    }

    if (bmvOk)
    {
        Serial.println("BMV080 data OK");
        Serial.printf("PM1: %.2f ug/m3\n", bmvPm1);
        Serial.printf("PM2.5: %.2f ug/m3\n", bmvPm2_5);
        Serial.printf("PM10: %.2f ug/m3\n", bmvPm10);
    }
    else
    {
        Serial.println("Failed to read BMV080");
        return;
    }

    sensorPoint.clearFields();
    sensorPoint.addField("opc_pm1", opcData.pm_a);
    sensorPoint.addField("opc_pm2_5", opcData.pm_b);
    sensorPoint.addField("opc_pm10", opcData.pm_c);
    sensorPoint.addField("opc_temperature", opcData.temperature_c);
    sensorPoint.addField("opc_humidity", opcData.humidity_rh);
    sensorPoint.addField("bmv_pm1", bmvPm1);
    sensorPoint.addField("bmv_pm2_5", bmvPm2_5);
    sensorPoint.addField("bmv_pm10", bmvPm10);
    sensorPoint.addField("bmv_obstruction", obstruction);
    sensorPoint.addField("bmv_out_of_range", outOfRange);
    sensorPoint.setTime();

    Serial.print("Writing to InfluxDB: ");
    Serial.println(client.pointToLineProtocol(sensorPoint));
    if (!client.writePoint(sensorPoint))
    {
        Serial.print("InfluxDB write failed: ");
        Serial.println(client.getLastErrorMessage());
    }
}


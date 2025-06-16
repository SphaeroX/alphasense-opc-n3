#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "config.h"
#include <time.h>

// Define the target CO2 concentration for calibration (in ppm)
const unsigned long measurementSleepMs = SENSOR_SLEEP_MS;
const unsigned long CALIBRATION_DELAY_MS = 300000; // 5 minutes

const uint16_t CALIBRATION_CO2_PPM = 424;
const uint8_t LED_PIN = 2; // ESP32 built-in LED (modify if needed)

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
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH); // Turn on LED during calibration

    Serial.begin(115200);
    while (!Serial)
        ;
    Serial.println("\n\nSCD41 Manual Calibration and Logging");

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

    // Inform the user to place the sensor in fresh air for calibration
    Serial.print("Place the sensor in fresh air (");
    Serial.print(CALIBRATION_CO2_PPM);
    Serial.print(" ppm CO2). Calibration will start in ");
    Serial.print(CALIBRATION_DELAY_MS / 60000);
    Serial.println(" minutes...");
}

void loop()
{
    static bool calibrationDone = false;
    static unsigned long startMs = millis();
    static unsigned long lastMeasurementMs = 0;

    if (!calibrationDone && millis() - startMs >= CALIBRATION_DELAY_MS)
    {
        // Start forced recalibration using the defined CO2 value
        Serial.print("Performing forced recalibration to ");
        Serial.print(CALIBRATION_CO2_PPM);
        Serial.println(" ppm...");
        scd4x.stopPeriodicMeasurement();
        uint16_t frcCorrection = 0;
        int16_t err = scd4x.performForcedRecalibration(CALIBRATION_CO2_PPM, frcCorrection);
        if (err == 0)
        {
            Serial.printf("Calibration successful, correction: %u ppm\n", frcCorrection);
            scd4x.persistSettings();
            // Turn off the LED after calibration is completed
            digitalWrite(LED_PIN, LOW);
        }
        else
        {
            Serial.printf("Calibration failed, error: %d\n", err);
            // If calibration fails, blink the LED to indicate an error
            while (true)
            {
                digitalWrite(LED_PIN, HIGH);
                delay(500);
                digitalWrite(LED_PIN, LOW);
                delay(500);
            }
        }
        scd4x.startPeriodicMeasurement();
        calibrationDone = true;
        Serial.println("Calibration finished. Starting normal operation.");
        return; // wait for next loop to start measurements
    }

    if (!calibrationDone)
    {
        return; // still waiting for calibration period to finish
    }

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

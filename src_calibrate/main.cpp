#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include "config.h"

// Define the target CO2 concentration for calibration (in ppm)
const uint16_t CALIBRATION_CO2_PPM = 424;
const uint8_t LED_PIN = 2; // ESP32 built-in LED (modify if needed)

SensirionI2cScd4x scd4x;

void setup()
{
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH); // Turn on LED during calibration

    Serial.begin(115200);
    while (!Serial)
        ;
    Serial.println("\n\nSCD41 Manual Calibration");

    Wire.begin();
    scd4x.begin(Wire, SCD41_I2C_ADDR_62);
    scd4x.wakeUp();
    scd4x.stopPeriodicMeasurement();
    scd4x.reinit();
    scd4x.startPeriodicMeasurement();

    // Inform the user to place the sensor in fresh air for calibration
    Serial.print("Place the sensor in fresh air (");
    Serial.print(CALIBRATION_CO2_PPM);
    Serial.println(" ppm CO2). Calibration will start in 5 minutes...");
}

void loop()
{
    static bool calibrationDone = false;
    static unsigned long startMs = millis();

    if (!calibrationDone && millis() - startMs >= 300000)
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
        Serial.println("Calibration finished. Restart device for normal operation.");
    }
}

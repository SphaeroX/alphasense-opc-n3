#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include "config.h"

SensirionI2cScd4x scd4x;

void setup() {
    Serial.begin(115200);
    while (!Serial);
    Serial.println("\n\nSCD41 Manual Calibration");

    Wire.begin();
    scd4x.begin(Wire, SCD41_I2C_ADDR_62);
    scd4x.wakeUp();
    scd4x.stopPeriodicMeasurement();
    scd4x.reinit();
    scd4x.startPeriodicMeasurement();

    Serial.println("Place the sensor in fresh air. Calibration will start in 60 seconds...");
}

void loop() {
    static bool calibrationDone = false;
    static unsigned long startMs = millis();

    if (!calibrationDone && millis() - startMs >= 60000) {
        Serial.println("Performing forced recalibration to 400 ppm...");
        scd4x.stopPeriodicMeasurement();
        uint16_t frcCorrection = 0;
        int16_t err = scd4x.performForcedRecalibration(400, frcCorrection);
        if (err == 0) {
            Serial.printf("Calibration successful, correction: %u ppm\n", frcCorrection);
            scd4x.persistSettings();
        } else {
            Serial.printf("Calibration failed, error: %d\n", err);
        }
        scd4x.startPeriodicMeasurement();
        calibrationDone = true;
        Serial.println("Calibration finished. Restart device for normal operation.");
    }
}

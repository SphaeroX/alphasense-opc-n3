#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "OpcN3.h"
#include "BMV080.h"
#include "config.h"
#include <Wire.h>
#include <time.h>

// --- Pin Configuration ---
const int OPC_MOSI_PIN = 23;
const int OPC_MISO_PIN = 19;
const int OPC_SCK_PIN = 18;
const int OPC_SS_PIN = 5;

// --- Sampling Period & Sleep Configuration ---
// Duration to wait between sensor readings (in milliseconds). Adjust this to
// control how long the OPC-N3 collects data before the next read. The waiting
// mechanism is non-blocking so the MCU can perform other tasks.
const unsigned long measurementSleepMs = SENSOR_SLEEP_MS;

// --- Global Objects ---
OpcN3 opc(OPC_SS_PIN);
BMV080 bmv;
const int MAX_CONSECUTIVE_FAILURES = 5;

#if defined(ESP32)
#define DEVICE "ESP32"
#else
#define DEVICE "ARDUINO"
#endif

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point sensorPoint("opc_bmv080");

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
  Serial.println("\n\nOPC-N3 + BMV080 Reader");

  // Connect to WiFi
  Serial.printf("Connecting to WiFi '%s'", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println(" connected");

  // Synchronize time for accurate timestamps
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  waitForTimeSync();

  // Prepare InfluxDB client
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

  // Initialize SPI bus
  SPI.begin(OPC_SCK_PIN, OPC_MISO_PIN, OPC_MOSI_PIN, OPC_SS_PIN);

  // Initialize I2C bus and BMV080 sensor
  Wire.begin();
  bmv.begin();
  bmv.startContinuous();



  // Initialize the OPC-N3 sensor
  if (!opc.begin())
  {
    Serial.println("FATAL: OPC-N3 initialization failed. Program halted.");
    while (1)
      ; // Halt execution
  }

}

void loop()
{
  static int consecutive_failures = 0;
  static bool discard_next_success = true; // discard first valid reading
  static unsigned long lastMeasurementMs = 0;

  unsigned long now = millis();
  if (now - lastMeasurementMs < measurementSleepMs)
  {
    return; // wait until the next measurement interval without blocking
  }
  lastMeasurementMs = now;

  Serial.println("\n--- Requesting New Measurement ---");

  OpcN3Data sensorData;
  float bmvPm1 = 0.0f, bmvPm2_5 = 0.0f, bmvPm10 = 0.0f;
  bool obstruction = false, outOfRange = false;

  bool opcOk = opc.readData(sensorData);
  bool bmvOk = bmv.readOutput(bmvPm1, bmvPm2_5, bmvPm10, obstruction, outOfRange);
  if (opcOk && bmvOk)
  {
    consecutive_failures = 0; // Reset counter on success

    if (discard_next_success)
    {
      Serial.println("First valid measurement discarded as per datasheet recommendation.");
      discard_next_success = false;
    }
    else
    {
      Serial.println("Data successfully read and validated.");
      Serial.printf("Temperature: %.2f C\n", sensorData.temperature_c);
      Serial.printf("Humidity: %.2f %%RH\n", sensorData.humidity_rh);

      Serial.printf("OPC PM1: %.2f ug/m3\n", sensorData.pm_a);
      Serial.printf("OPC PM2.5: %.2f ug/m3\n", sensorData.pm_b);
      Serial.printf("OPC PM10: %.2f ug/m3\n", sensorData.pm_c);
      Serial.printf("BMV PM1: %.2f ug/m3\n", bmvPm1);
      Serial.printf("BMV PM2.5: %.2f ug/m3\n", bmvPm2_5);
      Serial.printf("BMV PM10: %.2f ug/m3\n", bmvPm10);
      Serial.printf("Actual Sampling Period: %.2f s\n", sensorData.sampling_period_s); // Verify the period
      Serial.printf("Checksum: OK (Received: 0x%04X)\n", sensorData.received_checksum);

      // Print the individual bin counts with their size ranges
      Serial.println("\nParticle Size Bin Counts:");
      for (int i = 0; i < 24; i++)
      {
        Serial.printf("  Bin %2d (%.2f - %.2f um): %u counts\n",
                      i,
                      sensorData.bin_boundaries_um[i],
                      sensorData.bin_boundaries_um[i + 1],
                      sensorData.bin_counts[i]);
      }

      // Prepare InfluxDB point
      sensorPoint.clearFields();
      sensorPoint.addField("opc_pm1", sensorData.pm_a);
      sensorPoint.addField("opc_pm2_5", sensorData.pm_b);
      sensorPoint.addField("opc_pm10", sensorData.pm_c);
      sensorPoint.addField("opc_temperature", sensorData.temperature_c);
      sensorPoint.addField("opc_humidity", sensorData.humidity_rh);
      sensorPoint.addField("bmv_pm1", bmvPm1);
      sensorPoint.addField("bmv_pm2_5", bmvPm2_5);
      sensorPoint.addField("bmv_pm10", bmvPm10);

      // Add individual bin counts as separate fields for detailed analysis
      for (int i = 0; i < 24; i++)
      {
        char fieldName[12];
        snprintf(fieldName, sizeof(fieldName), "opc_bin_%02d", i);
        sensorPoint.addField(fieldName, (int)sensorData.bin_counts[i]);
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
  }
  else
  {
    consecutive_failures++;
    discard_next_success = true; // discard next successful measurement after a failure
    Serial.printf("Measurement failed (OPC: %s, BMV080: %s). This is failure #%d in a row.\n",
                  opcOk ? "ok" : "fail",
                  bmvOk ? "ok" : "fail",
                  consecutive_failures);

    if (consecutive_failures >= MAX_CONSECUTIVE_FAILURES)
    {
      Serial.println("WARNING: Multiple consecutive measurements failed. The sensor might have an issue or the connection is unstable.");
    }

    // Wait for recovery on failure
    delay(2500);
  }
}

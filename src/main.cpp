#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "OpcN3.h"
#include "config.h"
#include "OpenMeteoClient.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "DerivedMetrics.h"
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
SensirionI2cScd4x scd4x;
const int MAX_CONSECUTIVE_FAILURES = 5;
OpenMeteoClient openMeteo(WEATHER_LATITUDE, WEATHER_LONGITUDE, WEATHER_UPDATE_INTERVAL_MS);
OpenMeteoData latestWeatherData{};
TaskHandle_t weatherTaskHandle = nullptr;

#if defined(ESP32)
#define DEVICE "ESP32"
#else
#define DEVICE "ARDUINO"
#endif

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point sensorPoint("full");

// Wait until NTP has synchronized system time to a reasonable value
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

static void weatherTask(void *pvParameters)
{
  (void)pvParameters;
  for (;;) {
    if (openMeteo.update()) {
      latestWeatherData = openMeteo.data();
    }
    vTaskDelay(pdMS_TO_TICKS(WEATHER_UPDATE_INTERVAL_MS));
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;
  Serial.println("\n\nOPC-N3 Sensor Reader - Structured Version");

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

  // Fetch initial weather data
  if (openMeteo.update()) {
    latestWeatherData = openMeteo.data();
  }

  // Start asynchronous weather updates
  xTaskCreatePinnedToCore(weatherTask, "WeatherTask", 8192, nullptr, 1, &weatherTaskHandle, 1);

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

  // Initialize I2C bus for the SCD41 gas sensor
  Wire.begin();
  scd4x.begin(Wire, SCD41_I2C_ADDR_62);
  scd4x.wakeUp();
  scd4x.stopPeriodicMeasurement();
  scd4x.reinit();
  scd4x.startPeriodicMeasurement();

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

  // Weather data is updated asynchronously

  OpcN3Data sensorData;
  if (opc.readData(sensorData))
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

      // Read SCD41 gas sensor values
      bool scdReady = false;
      uint16_t co2 = 0;
      float scdTemperature = 0.0f;
      float scdHumidity = 0.0f;
      int16_t scdError = scd4x.getDataReadyStatus(scdReady);
      if (scdError == 0 && scdReady)
      {
        scdError = scd4x.readMeasurement(co2, scdTemperature, scdHumidity);
        if (scdError == 0)
        {
          Serial.printf("CO2: %u ppm\n", co2);
          Serial.printf("SCD Temperature: %.2f C\n", scdTemperature);
          Serial.printf("SCD Humidity: %.2f %%RH\n", scdHumidity);
        }
        else
        {
          Serial.println("Error reading SCD41 measurement");
        }
      }
      else if (scdError != 0)
      {
        Serial.println("Error checking SCD41 data ready status");
      }
      Serial.printf("PM1: %.2f ug/m3\n", sensorData.pm_a);
      Serial.printf("PM2.5: %.2f ug/m3\n", sensorData.pm_b);
      Serial.printf("PM10: %.2f ug/m3\n", sensorData.pm_c);
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

      // Derived metrics
      uint32_t pollenCount = calculatePollenCount(sensorData);
      uint8_t pollenLevel = classifyPollenLevel(pollenCount);
      uint8_t co2Quality = classifyCo2Quality(co2);
      Serial.printf("Pollen count: %u\n", pollenCount);
      Serial.printf("Pollen level: %s (%u)\n", pollenLevelName(pollenLevel), pollenLevel);
      Serial.printf("CO2 quality: %s (%u)\n", co2QualityName(co2Quality), co2Quality);

      // Prepare InfluxDB point
      sensorPoint.clearFields();
      sensorPoint.addField("opc_pm1", sensorData.pm_a);
      sensorPoint.addField("opc_pm2_5", sensorData.pm_b);
      sensorPoint.addField("opc_pm10", sensorData.pm_c);
      sensorPoint.addField("opc_temperature", sensorData.temperature_c);
      sensorPoint.addField("opc_humidity", sensorData.humidity_rh);
      sensorPoint.addField("scd41_co2", co2);
      sensorPoint.addField("scd41_temperature", scdTemperature);
      sensorPoint.addField("scd41_humidity", scdHumidity);
      sensorPoint.addField("calc_pollen_count", (int)pollenCount);
      sensorPoint.addField("calc_pollen_level", pollenLevel);
      sensorPoint.addField("calc_co2_quality", co2Quality);

      if (latestWeatherData.valid)
      {
        sensorPoint.addField("weather_temperature", latestWeatherData.temperature_c);
        sensorPoint.addField("weather_humidity", latestWeatherData.humidity_rh);
        sensorPoint.addField("weather_apparent_temperature", latestWeatherData.apparent_temperature_c);
        sensorPoint.addField("weather_is_day", latestWeatherData.is_day);
        sensorPoint.addField("weather_rain", latestWeatherData.rain_mm);
        sensorPoint.addField("weather_cloud_cover_pct", latestWeatherData.cloud_cover_pct);
        sensorPoint.addField("weather_pressure_msl", latestWeatherData.pressure_msl_hpa);
        sensorPoint.addField("weather_surface_pressure", latestWeatherData.surface_pressure_hpa);
        sensorPoint.addField("weather_wind_speed_kmh", latestWeatherData.wind_speed_kmh);
        sensorPoint.addField("weather_wind_dir_deg", latestWeatherData.wind_direction_deg);
        sensorPoint.addField("weather_wind_gusts_kmh", latestWeatherData.wind_gusts_kmh);
        sensorPoint.addField("air_ragweed_pollen", latestWeatherData.ragweed_pollen_grains_m3);
        sensorPoint.addField("air_olive_pollen", latestWeatherData.olive_pollen_grains_m3);
        sensorPoint.addField("air_mugwort_pollen", latestWeatherData.mugwort_pollen_grains_m3);
        sensorPoint.addField("air_grass_pollen", latestWeatherData.grass_pollen_grains_m3);
        sensorPoint.addField("air_birch_pollen", latestWeatherData.birch_pollen_grains_m3);
        sensorPoint.addField("air_alder_pollen", latestWeatherData.alder_pollen_grains_m3);
        sensorPoint.addField("air_dust", latestWeatherData.dust_ug_m3);
        sensorPoint.addField("air_carbon_monoxide", latestWeatherData.carbon_monoxide_ug_m3);
        sensorPoint.addField("air_pm2_5", latestWeatherData.pm2_5_ug_m3);
        sensorPoint.addField("air_pm10", latestWeatherData.pm10_ug_m3);
        sensorPoint.addField("air_european_aqi", latestWeatherData.european_aqi);
      }

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
    Serial.printf("Measurement failed. This is failure #%d in a row.\n", consecutive_failures);

    if (consecutive_failures >= MAX_CONSECUTIVE_FAILURES)
    {
      Serial.println("WARNING: Multiple consecutive measurements failed. The sensor might have an issue or the connection is unstable.");
    }

    // Wait for recovery on failure
    delay(2500);
  }
}

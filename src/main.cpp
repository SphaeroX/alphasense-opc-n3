#include <Arduino.h>
#include <SPI.h>
#include "OpcN3.h" // Include our new OPC-N3 library

// --- Pin Configuration ---
const int OPC_MOSI_PIN = 23;
const int OPC_MISO_PIN = 19;
const int OPC_SCK_PIN = 18;
const int OPC_SS_PIN = 5;

// --- Global Objects ---
OpcN3 opc(OPC_SS_PIN);
const int MAX_CONSECUTIVE_FAILURES = 5;

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;
  Serial.println("\n\nOPC-N3 Sensor Reader - Structured Version");

  // Initialize SPI bus
  SPI.begin(OPC_SCK_PIN, OPC_MISO_PIN, OPC_MOSI_PIN, OPC_SS_PIN);

  // Initialize the OPC-N3 sensor
  // This now includes setting the default sampling period to 1 second
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

  Serial.println("\n--- Requesting New Measurement ---");

  OpcN3Data sensorData;
  if (opc.readData(sensorData))
  {
    consecutive_failures = 0; // Reset counter on success
    Serial.println("Data successfully read and validated.");
    Serial.printf("Temperature: %.2f C\n", sensorData.temperature_c);
    Serial.printf("Humidity: %.2f %%RH\n", sensorData.humidity_rh);
    Serial.printf("PM1: %.2f ug/m3\n", sensorData.pm_a);
    Serial.printf("PM2.5: %.2f ug/m3\n", sensorData.pm_b);
    Serial.printf("PM10: %.2f ug/m3\n", sensorData.pm_c);
    Serial.printf("Actual Sampling Period: %.2f s\n", sensorData.sampling_period_s); // Verify the period
    Serial.printf("Checksum: OK (Received: 0x%04X)\n", sensorData.received_checksum);

    // Print the individual bin counts with their size ranges
    Serial.println("\nParticle Size Bin Counts:");
    for (int i = 0; i < 16; i++)
    {
      Serial.printf("  Bin %2d (%.2f - %.2f um): %u counts\n",
                    i,
                    sensorData.bin_boundaries_um[i],
                    sensorData.bin_boundaries_um[i + 1],
                    sensorData.bin_counts[i]);
    }
  }
  else
  {
    consecutive_failures++;
    Serial.printf("Measurement failed. This is failure #%d in a row.\n", consecutive_failures);

    if (consecutive_failures >= MAX_CONSECUTIVE_FAILURES)
    {
      Serial.println("WARNING: Multiple consecutive measurements failed. The sensor might have an issue or the connection is unstable.");
    }

    // Wait for recovery on failure
    delay(2500);
  }

  // Wait for the next measurement cycle
  // The delay here should be longer than the set sampling period
  delay(5000);
}
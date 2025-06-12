# OPC-N3 Arduino Library for Platform.io

This is a robust, structured Arduino library for the Alphasense OPC-N3 optical particle counter, designed for use with the Platform.io IDE. It provides a clean, high-level interface for initializing the sensor, reading particle data, and configuring key parameters. The library is built with a focus on reliability, implementing retry logic and error handling as recommended by the official datasheets.

The library automatically transfers all measurements to InfluxDB via WiFi connection, enabling seamless data logging and monitoring. This integration allows for real-time data visualization and analysis through platforms like Grafana.

## Features

- **Structured Code**: The logic is encapsulated in an `OpcN3` class, keeping your `main.cpp` clean and simple.
- **Robust Initialization**: Implements a retry mechanism for all critical startup steps (connection check, fan/laser power-on) to handle temporary communication issues.
- **CRC Checksum Validation**: Automatically validates every data packet using the 16-bit CRC checksum to ensure data integrity.
- **Detailed Data Parsing**: Reads and parses the full histogram dataset, including:
    - PM1, PM2.5, and PM10 mass concentrations (in µg/m³).
    - Temperature (in °C) and Relative Humidity (in %RH).
    - Particle counts for 16 distinct size bins.
    - Particle size boundaries for each bin (in µm).
    - Actual sampling period and sample flow rate.
    - Status information like laser status and reject counts.
- **Configurable Sampling Period**: Allows you to programmatically set the sensor's sampling period.
- **Clear Serial Output**: Provides detailed, human-readable logs for initialization, measurements, and error conditions.

## Hardware Requirements

- An Alphasense OPC-N3 sensor.
- A microcontroller compatible with Arduino (e.g., ESP32, ESP8266, Arduino Uno).
- An SPI interface connection between the microcontroller and the OPC-N3.

## Quickstart Example

The following example shows how to initialize the sensor and continuously read data.

```cpp
// src/main.cpp

#include <Arduino.h>
#include <SPI.h>
#include "OpcN3.h" // Include the OPC-N3 library

// --- Pin Configuration ---
const int OPC_MOSI_PIN = 23;
const int OPC_MISO_PIN = 19;
const int OPC_SCK_PIN = 18;
const int OPC_SS_PIN = 5;

// --- Global Objects ---
OpcN3 opc(OPC_SS_PIN);

void setup() {
    Serial.begin(115200);
    while (!Serial);
    Serial.println("\nOPC-N3 Sensor Reader Initializing...");

    // Initialize the SPI bus
    SPI.begin(OPC_SCK_PIN, OPC_MISO_PIN, OPC_MOSI_PIN, OPC_SS_PIN);

    // Initialize the OPC-N3 sensor. This handles power-on,
    // configuration reading, and sets a default 1-second sampling period.
    if (!opc.begin()) {
        Serial.println("FATAL: OPC-N3 initialization failed. Program halted.");
        while (1); // Halt execution
    }
}

void loop() {
    Serial.println("\n--- Requesting New Measurement ---");
    
    OpcN3Data sensorData;
    if (opc.readData(sensorData)) {
        Serial.println("Data successfully read and validated.");
        Serial.printf("Temperature: %.2f C\n", sensorData.temperature_c);
        Serial.printf("Humidity: %.2f %%RH\n", sensorData.humidity_rh);
        Serial.printf("PM1: %.2f ug/m3\n", sensorData.pm_a);
        Serial.printf("PM2.5: %.2f ug/m3\n", sensorData.pm_b);
        Serial.printf("PM10: %.2f ug/m3\n", sensorData.pm_c);
        Serial.printf("Actual Sampling Period: %.2f s\n", sensorData.sampling_period_s);

        // Print the individual bin counts with their size ranges
        Serial.println("\nParticle Size Bin Counts:");
        for (int i = 0; i < 16; i++) {
            Serial.printf("  Bin %2d (%.2f - %.2f um): %u counts\n", 
                i, 
                sensorData.bin_boundaries_um[i], 
                sensorData.bin_boundaries_um[i + 1], 
                sensorData.bin_counts[i]);
        }
    } else {
        Serial.println("Measurement failed. Retrying after a recovery delay.");
        delay(2500); // Wait before next attempt
    }

    // Wait for the next measurement cycle
    delay(5000); 
}
```

## API Reference

### `OpcN3` Class

#### `OpcN3(int ss_pin)`
Constructor. Creates an instance of the OPC-N3 driver.
- **Parameters**:
    - `ss_pin`: The GPIO pin connected to the sensor's Slave Select (/SS) line.

#### `bool begin()`
Initializes the sensor. This method performs the full startup sequence:
1.  Waits for the sensor to power up.
2.  Establishes and verifies the SPI connection by reading the firmware version.
3.  Turns on the fan and laser, with appropriate delays.
4.  Reads the sensor's configuration, including particle bin boundaries.
5.  Sets a default sampling period of 1 second.
- **Returns**: `true` if initialization was successful, `false` otherwise.

#### `bool readData(OpcN3Data &data)`
Reads the latest histogram data packet from the sensor, validates the CRC, and populates the provided data structure.
- **Parameters**:
    - `data`: A reference to an `OpcN3Data` struct where the results will be stored.
- **Returns**: `true` if data was read and validated successfully, `false` on communication error or CRC mismatch.

#### `bool setSamplingPeriod(float seconds)`
Sets the active sampling period of the sensor. This modifies the `AMSamplingIntervalCount` configuration variable in the sensor's volatile memory. The change will be reset on power loss.
- **Parameters**:
    - `seconds`: The desired sampling period in seconds. The recommended range is 1.0 to 30.0.
- **Returns**: `true` if the configuration was successfully written to the sensor, `false` otherwise.

### `OpcN3Data` Struct

This struct holds all the values read from the sensor.

| Member                  | Type           | Description                                                              |
| :---------------------- | :------------- | :----------------------------------------------------------------------- |
| `bin_counts[16]`        | `uint16_t`     | Array containing the particle counts for each of the 16 main bins.       |
| `bin_boundaries_um[25]` | `float`        | Array of particle diameters (in µm) defining the edges of the 24 bins.   |
| `pm_a`                  | `float`        | Mass concentration for PM size A (typically PM1) in µg/m³.               |
| `pm_b`                  | `float`        | Mass concentration for PM size B (typically PM2.5) in µg/m³.             |
| `pm_c`                  | `float`        | Mass concentration for PM size C (typically PM10) in µg/m³.              |
| `temperature_c`         | `float`        | Ambient temperature in degrees Celsius.                                  |
| `humidity_rh`           | `float`        | Relative humidity in percent (%RH).                                      |
| `sampling_period_s`     | `float`        | The actual sampling period reported by the sensor in seconds.            |
| `sample_flow_rate_ml_s` | `float`        | The sample flow rate reported by the sensor in mL/s.                     |
| `checksum_ok`           | `bool`         | `true` if the received CRC checksum matched the calculated one.          |
| `received_checksum`     | `uint16_t`     | The 16-bit CRC checksum value received from the sensor.                  |
| `laser_status`          | `uint16_t`     | Status indicator for the laser. A value between 550-650 is typical.      |
| `fan_rev_count`         | `uint16_t`     | Fan revolution counter (currently unused in standard operation).         |
| `reject_count_glitch`   | `uint16_t`     | Count of particles rejected due to electronic noise.                     |
| `reject_count_long_tof` | `uint16_t`     | Count of particles rejected for taking too long to cross the laser.      |
| `reject_count_ratio`    | `uint16_t`     | Count of particles rejected based on internal validation ratios.         |

## Troubleshooting

- **No Connection / Initialization Fails**:
    - Double-check your wiring. Ensure MOSI, MISO, SCK, and SS are correctly connected.
    - Verify that the sensor is receiving adequate power (4.8V to 5.2V).
    - Make sure the `OPC_SS_PIN` in `main.cpp` matches your wiring.
- **CRC Mismatch Errors**:
    - This can indicate a noisy SPI line or incorrect timing. Ensure your connections are solid and wires are not excessively long.
    - Check if the SPI clock speed (`SPI_CLOCK_SPEED` in `OpcN3.cpp`) is within the recommended range (300-750 kHz).
- **Data Seems Incorrect**:
    - The first data set after power-on or an error should be discarded, as per the datasheet. This library handles the communication, but your application logic might need to account for this initial reading.
    - High humidity (>85% RH) can affect readings, causing particles to appear larger. This is a physical property of aerosols, not a sensor fault.

## InfluxDB Integration
The example project writes all sensor data directly to an InfluxDB instance.
In addition to the PM values and environmental measurements, the individual
particle bin counts are stored as separate fields (`bin_00` to `bin_15`). This
allows detailed analysis and visualization of the histogram data in tools like
Grafana.

## License

This project is open-source. Please feel free to use, modify, and distribute it. See the `LICENSE` file for details.
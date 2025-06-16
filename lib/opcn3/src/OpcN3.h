#ifndef OPCN3_H
#define OPCN3_H

#include <Arduino.h>
#include <SPI.h>

// Data structure to hold all the readings from the OPC-N3
struct OpcN3Data
{
    // Histogram Data
    // The OPC-N3 reports 24 histogram bins
    uint16_t bin_counts[24];
    float bin_boundaries_um[25];

    // MToF (Time-of-Flight for specific bins)
    uint8_t bin1_mtof, bin3_mtof, bin5_mtof, bin7_mtof;

    // Converted Values
    float sampling_period_s, sample_flow_rate_ml_s, temperature_c, humidity_rh;

    // PM Values
    float pm_a, pm_b, pm_c;

    // Status and Error Values
    uint16_t reject_count_glitch, reject_count_long_tof, reject_count_ratio;
    uint16_t fan_rev_count, laser_status;

    // Checksum
    uint16_t received_checksum;
    bool checksum_ok;
};

class OpcN3
{
public:
    OpcN3(int ss_pin);

    // Initializes the sensor (SPI, connection check, power on, config read, set default period)
    bool begin();

    // Reads the latest histogram data from the sensor
    bool readData(OpcN3Data &data);

private:
    // --- Pin and SPI settings ---
    int _ss_pin;
    SPISettings _spi_settings;

    // --- Internal state ---
    uint8_t _config_vars[168]; // Buffer to hold the full configuration variables

    // --- Helper methods for SPI communication ---
    bool waitForReady(uint8_t cmd, int timeout_ms = 500);
    bool sendCommandWithData(uint8_t cmd, uint8_t data);

    // --- Helper methods for sensor control ---
    bool powerControl(uint8_t option);
    bool checkConnection();
    bool readConfiguration();
    bool writeConfiguration(); // NEW: Writes the configuration variables back to the sensor

    // --- Helper methods for data processing ---
    uint16_t crc16_calc(const uint8_t *data, size_t len);
    uint16_t combine_bytes(uint8_t lsb, uint8_t msb);
    float bytes_to_float(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3);
};

#endif // OPCN3_H
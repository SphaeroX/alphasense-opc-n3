#pragma once

#include <Arduino.h>
#include <SPI.h>

class OpcN3
{
public:
    // Konstruktor
    OpcN3(int mosiPin, int misoPin, int sckPin, int ssPin);

    // Initialisierung
    bool begin();

    // Datenstruktur für Messwerte
    struct Data
    {
        uint16_t bin_counts[16];
        uint8_t bin1_mtof, bin3_mtof, bin5_mtof, bin7_mtof;
        float sampling_period_s, sample_flow_rate_ml_s, temperature_c, humidity_rh;
        float pm_a, pm_b, pm_c;
        uint16_t reject_count_glitch, reject_count_long_tof, reject_count_ratio;
        uint16_t fan_rev_count, laser_status;
        uint16_t received_checksum;
        bool checksum_ok;
    };

    // Hauptfunktionen
    bool readHistogramData(Data &data);
    bool checkConnection();

private:
    // Pin-Konfiguration
    const int _mosiPin;
    const int _misoPin;
    const int _sckPin;
    const int _ssPin;

    // SPI-Einstellungen
    static const uint32_t SPI_CLOCK_SPEED = 500000;
    const SPISettings _spiSettings;

    // Befehle
    static const uint8_t CMD_CHECK_STATUS = 0xCF;
    static const uint8_t CMD_READ_FIRMWARE = 0x12;
    static const uint8_t CMD_POWER_CONTROL = 0x03;
    static const uint8_t CMD_READ_HISTOGRAM = 0x30;

    // Power Control Optionen
    static const uint8_t POWER_FAN_ON = 0x03;
    static const uint8_t POWER_FAN_OFF = 0x02;
    static const uint8_t POWER_LASER_ON = 0x07;
    static const uint8_t POWER_LASER_OFF = 0x06;

    // Antworten
    static const uint8_t RESP_READY = 0xF3;
    static const uint8_t RESP_BUSY = 0x31;

    // Timing und Robustheit
    static const int DELAY_CMD_POLLING_MS = 10;
    static const int DELAY_INTER_BYTE_US = 10;
    static const int DELAY_FAN_ON_MS = 1000;
    static const int DELAY_LASER_ON_MS = 200;
    static const int DELAY_CMD_RECOVERY_MS = 2500;
    static const int MAX_INIT_RETRIES = 5;

    // Hilfsfunktionen
    bool waitForReady(uint8_t cmd, int timeout_ms = 500);
    bool sendCommandWithData(uint8_t cmd, uint8_t data);
    bool powerControl(uint8_t option);
    static uint16_t combine_bytes(uint8_t lsb, uint8_t msb);
    static float bytes_to_float(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3);
    static uint16_t crc16_calc(const uint8_t *data, size_t len);
};

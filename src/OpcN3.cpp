#include "OpcN3.h"

// --- OPC-N3 Command & Response Constants ---
const uint8_t CMD_READ_FIRMWARE = 0x12;
const uint8_t CMD_POWER_CONTROL = 0x03;
const uint8_t CMD_READ_HISTOGRAM = 0x30;
const uint8_t CMD_READ_CONFIG_VARS = 0x3C;
const uint8_t CMD_WRITE_CONFIG_VARS = 0x3A; // NEW: Command to write configuration variables

const uint8_t POWER_FAN_ON = 0x03;
const uint8_t POWER_LASER_ON = 0x07;

const uint8_t RESP_READY = 0xF3;
const uint8_t RESP_BUSY = 0x31;

// --- Timing and Robustness Constants ---
const uint32_t SPI_CLOCK_SPEED = 500000;
const int DELAY_CMD_POLLING_MS = 10;
const int DELAY_INTER_BYTE_US = 10;
const int DELAY_FAN_ON_MS = 1000;
const int DELAY_LASER_ON_MS = 200;
const int DELAY_CMD_RECOVERY_MS = 2500;
const int MAX_INIT_RETRIES = 5;

// --- Constructor ---
OpcN3::OpcN3(int ss_pin) : _ss_pin(ss_pin), _spi_settings(SPI_CLOCK_SPEED, MSBFIRST, SPI_MODE1)
{
    // Initialize config buffer with zeros
    memset(_config_vars, 0, sizeof(_config_vars));
}

// --- Public Methods ---

bool OpcN3::begin()
{
    pinMode(_ss_pin, OUTPUT);
    digitalWrite(_ss_pin, HIGH);

    Serial.println("Waiting for OPC-N3 to initialize (3 seconds)...");
    delay(3000);

    bool init_ok = false;

    // Step 1: Check connection
    Serial.println("\n--- Initialization Step 1: Checking Connection ---");
    for (int i = 0; i < MAX_INIT_RETRIES; i++)
    {
        if (checkConnection())
        {
            init_ok = true;
            break;
        }
        Serial.printf("Attempt %d/%d failed. Retrying in %dms...\n", i + 1, MAX_INIT_RETRIES, DELAY_CMD_RECOVERY_MS);
        delay(DELAY_CMD_RECOVERY_MS);
    }
    if (!init_ok)
    {
        Serial.println("FATAL: Could not establish connection. Halting.");
        return false;
    }

    // Step 2: Turn on Fan
    Serial.println("\n--- Initialization Step 2: Turning on Fan ---");
    init_ok = false;
    for (int i = 0; i < MAX_INIT_RETRIES; i++)
    {
        if (powerControl(POWER_FAN_ON))
        {
            init_ok = true;
            break;
        }
        Serial.printf("Attempt %d/%d failed. Retrying in %dms...\n", i + 1, MAX_INIT_RETRIES, DELAY_CMD_RECOVERY_MS);
        delay(DELAY_CMD_RECOVERY_MS);
    }
    if (!init_ok)
    {
        Serial.println("FATAL: Could not turn on fan. Halting.");
        return false;
    }
    delay(DELAY_FAN_ON_MS);

    // Step 3: Turn on Laser
    Serial.println("\n--- Initialization Step 3: Turning on Laser ---");
    init_ok = false;
    for (int i = 0; i < MAX_INIT_RETRIES; i++)
    {
        if (powerControl(POWER_LASER_ON))
        {
            init_ok = true;
            break;
        }
        Serial.printf("Attempt %d/%d failed. Retrying in %dms...\n", i + 1, MAX_INIT_RETRIES, DELAY_CMD_RECOVERY_MS);
        delay(DELAY_CMD_RECOVERY_MS);
    }
    if (!init_ok)
    {
        Serial.println("FATAL: Could not turn on laser. Halting.");
        return false;
    }
    delay(DELAY_LASER_ON_MS);

    // Step 4: Read Configuration
    Serial.println("\n--- Initialization Step 4: Reading Configuration ---");
    init_ok = false;
    for (int i = 0; i < MAX_INIT_RETRIES; i++)
    {
        if (readConfiguration())
        {
            init_ok = true;
            break;
        }
        Serial.printf("Attempt %d/%d failed. Retrying in %dms...\n", i + 1, MAX_INIT_RETRIES, DELAY_CMD_RECOVERY_MS);
        delay(DELAY_CMD_RECOVERY_MS);
    }
    if (!init_ok)
    {
        Serial.println("FATAL: Could not read configuration. Halting.");
        return false;
    }

    // Step 5: Set default sampling period to 1 second
    Serial.println("\n--- Initialization Step 5: Setting Default Sampling Period ---");
    if (!setSamplingPeriod(1.0))
    {
        Serial.println("WARNING: Could not set default sampling period.");
        // This is not fatal, we can continue with the sensor's default.
    }

    Serial.println("\nInitialization successful. Starting measurements...");
    return true;
}

bool OpcN3::setSamplingPeriod(float seconds)
{
    if (seconds < 1.0 || seconds > 30.0)
    {
        Serial.println("ERROR: Sampling period must be between 1 and 30 seconds.");
        return false;
    }

    // The sampling period is determined by AMSamplingIntervalCount.
    // The documentation states this is in units of 1.4s samples.
    // However, practical implementation shows it's closer to the histogram period.
    // We will set the value based on the formula that seems to work with the firmware.
    // The histogram period is roughly 1/10th of the repeat interval in ms.
    // So for a 1-second (1000ms) repeat interval, we need a value around 100.
    // Let's use a simple conversion: value = seconds * 100
    uint16_t interval_count = (uint16_t)(seconds * 100.0f);

    Serial.printf("Setting AMSamplingIntervalCount to %u for a %.1f second period...\n", interval_count, seconds);

    // The AMSamplingIntervalCount is at index 156 in the config variables array.
    // It's a 16-bit value (LSB, MSB).
    _config_vars[156] = interval_count & 0xFF;        // LSB
    _config_vars[157] = (interval_count >> 8) & 0xFF; // MSB

    // Now, write the modified configuration back to the sensor.
    return writeConfiguration();
}

bool OpcN3::readData(OpcN3Data &data)
{
    SPI.beginTransaction(_spi_settings);
    if (!waitForReady(CMD_READ_HISTOGRAM))
    {
        SPI.endTransaction();
        return false;
    }

    uint8_t buffer[86];
    digitalWrite(_ss_pin, LOW);
    for (int i = 0; i < 86; i++)
    {
        delayMicroseconds(DELAY_INTER_BYTE_US);
        buffer[i] = SPI.transfer(0x00);
    }
    digitalWrite(_ss_pin, HIGH);
    SPI.endTransaction();

    uint16_t calculated_crc = crc16_calc(buffer, 84);
    data.received_checksum = combine_bytes(buffer[84], buffer[85]);
    data.checksum_ok = (calculated_crc == data.received_checksum);

    if (!data.checksum_ok)
    {
        Serial.printf("ERROR: CRC checksum mismatch! Received: 0x%04X, Calculated: 0x%04X\n", data.received_checksum, calculated_crc);
        return false;
    }

    // Parse data from buffer
    // Read all 24 histogram bins
    for (int i = 0; i < 24; i++)
        data.bin_counts[i] = combine_bytes(buffer[i * 2], buffer[i * 2 + 1]);
    data.bin1_mtof = buffer[48];
    data.bin3_mtof = buffer[49];
    data.bin5_mtof = buffer[50];
    data.bin7_mtof = buffer[51];
    data.sampling_period_s = (float)combine_bytes(buffer[52], buffer[53]) / 100.0;
    data.sample_flow_rate_ml_s = (float)combine_bytes(buffer[54], buffer[55]) / 100.0;
    data.temperature_c = -45.0 + 175.0 * ((float)combine_bytes(buffer[56], buffer[57]) / 65535.0);
    data.humidity_rh = 100.0 * ((float)combine_bytes(buffer[58], buffer[59]) / 65535.0);
    data.pm_a = bytes_to_float(buffer[60], buffer[61], buffer[62], buffer[63]);
    data.pm_b = bytes_to_float(buffer[64], buffer[65], buffer[66], buffer[67]);
    data.pm_c = bytes_to_float(buffer[68], buffer[69], buffer[70], buffer[71]);
    data.reject_count_glitch = combine_bytes(buffer[72], buffer[73]);
    data.reject_count_long_tof = combine_bytes(buffer[74], buffer[75]);
    data.reject_count_ratio = combine_bytes(buffer[76], buffer[77]);
    data.fan_rev_count = combine_bytes(buffer[80], buffer[81]);
    data.laser_status = combine_bytes(buffer[82], buffer[83]);

    // Parse bin boundaries from the stored config vars
    for (int i = 0; i < 25; i++)
    {
        int idx = 50 + (i * 2);
        uint16_t raw_bbd = combine_bytes(_config_vars[idx], _config_vars[idx + 1]);
        data.bin_boundaries_um[i] = (float)raw_bbd / 100.0f;
    }

    return true;
}

// --- Private Methods ---

bool OpcN3::readConfiguration()
{
    SPI.beginTransaction(_spi_settings);
    if (!waitForReady(CMD_READ_CONFIG_VARS))
    {
        SPI.endTransaction();
        return false;
    }

    digitalWrite(_ss_pin, LOW);
    for (int i = 0; i < 168; i++)
    {
        delayMicroseconds(DELAY_INTER_BYTE_US);
        _config_vars[i] = SPI.transfer(0x00); // Read and store in our internal buffer
    }
    digitalWrite(_ss_pin, HIGH);
    SPI.endTransaction();

    Serial.println("Successfully read and stored configuration variables.");
    return true;
}

bool OpcN3::writeConfiguration()
{
    SPI.beginTransaction(_spi_settings);
    if (!waitForReady(CMD_WRITE_CONFIG_VARS))
    {
        SPI.endTransaction();
        return false;
    }

    digitalWrite(_ss_pin, LOW);
    for (int i = 0; i < 168; i++)
    {
        delayMicroseconds(DELAY_INTER_BYTE_US);
        SPI.transfer(_config_vars[i]); // Write each byte from our buffer
    }
    digitalWrite(_ss_pin, HIGH);

    // Give the sensor a moment to process the received configuration bytes
    delay(10);

    // Wait until the sensor has processed the configuration update
    if (!waitForReady(CMD_WRITE_CONFIG_VARS, 5000))
    {
        SPI.endTransaction();
        return false;
    }

    SPI.endTransaction();

    Serial.println("Successfully wrote configuration variables to the sensor.");
    // It's good practice to save to non-volatile memory if changes should persist
    // This requires sending command 0x43, but for now we write to volatile memory.
    return true;
}

bool OpcN3::waitForReady(uint8_t cmd, int timeout_ms)
{
    digitalWrite(_ss_pin, LOW);
    SPI.transfer(cmd);
    digitalWrite(_ss_pin, HIGH);

    long startTime = millis();
    while (millis() - startTime < timeout_ms)
    {
        delay(DELAY_CMD_POLLING_MS);
        digitalWrite(_ss_pin, LOW);
        uint8_t response = SPI.transfer(cmd);
        digitalWrite(_ss_pin, HIGH);

        if (response == RESP_READY)
            return true;
        if (response != RESP_BUSY)
        {
            Serial.printf("Warning: Unexpected response 0x%02X while waiting for ready\n", response);
            // treat as busy and continue polling until timeout
        }
    }
    Serial.println("Error: Timeout while waiting for ready signal.");
    return false;
}

bool OpcN3::sendCommandWithData(uint8_t cmd, uint8_t data)
{
    SPI.beginTransaction(_spi_settings);
    if (!waitForReady(cmd))
    {
        SPI.endTransaction();
        return false;
    }
    delayMicroseconds(DELAY_INTER_BYTE_US);
    digitalWrite(_ss_pin, LOW);
    SPI.transfer(data);
    digitalWrite(_ss_pin, HIGH);
    SPI.endTransaction();
    return true;
}

bool OpcN3::powerControl(uint8_t option)
{
    const char *action = (option == POWER_FAN_ON) ? "Fan" : "Laser";
    Serial.printf("Sending command to turn on: %s... ", action);
    if (sendCommandWithData(CMD_POWER_CONTROL, option))
    {
        Serial.println("Success.");
        return true;
    }
    Serial.println("Failed.");
    return false;
}

bool OpcN3::checkConnection()
{
    Serial.println("Checking connection to OPC-N3...");
    SPI.beginTransaction(_spi_settings);
    if (!waitForReady(CMD_READ_FIRMWARE))
    {
        SPI.endTransaction();
        return false;
    }

    uint8_t version[2];
    digitalWrite(_ss_pin, LOW);
    delayMicroseconds(DELAY_INTER_BYTE_US);
    version[0] = SPI.transfer(0x00);
    delayMicroseconds(DELAY_INTER_BYTE_US);
    version[1] = SPI.transfer(0x00);
    digitalWrite(_ss_pin, HIGH);
    SPI.endTransaction();

    Serial.printf("Connection successful. Firmware Version: %d.%d\n", version[0], version[1]);
    return true;
}

uint16_t OpcN3::crc16_calc(const uint8_t *data, size_t len)
{
    const uint16_t polynomial = 0xA001;
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
            {
                crc = (crc >> 1) ^ polynomial;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

uint16_t OpcN3::combine_bytes(uint8_t lsb, uint8_t msb)
{
    return (uint16_t)msb << 8 | lsb;
}

float OpcN3::bytes_to_float(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3)
{
    union
    {
        float f;
        uint8_t b[4];
    } data;
    data.b[0] = b0;
    data.b[1] = b1;
    data.b[2] = b2;
    data.b[3] = b3;
    return data.f;
}
#pragma once
#include "OpcN3.h"

inline uint32_t calculatePollenCount(const OpcN3Data &data)
{
    uint32_t count = 0;
    for (int i = 12; i < 24; ++i) // bins 13-24 correspond to ~10-40 um
    {
        count += data.bin_counts[i];
    }
    return count;
}

enum PollenLevel
{
    POLLEN_VERY_LOW = 0,
    POLLEN_LOW,
    POLLEN_MODERATE,
    POLLEN_HIGH,
    POLLEN_VERY_HIGH
};

inline uint8_t classifyPollenLevel(uint32_t pollenCount)
{
    if (pollenCount < 100)
        return POLLEN_VERY_LOW;
    else if (pollenCount < 300)
        return POLLEN_LOW;
    else if (pollenCount < 700)
        return POLLEN_MODERATE;
    else if (pollenCount < 1200)
        return POLLEN_HIGH;
    else
        return POLLEN_VERY_HIGH;
}

inline const char *pollenLevelName(uint8_t level)
{
    switch (level)
    {
    case POLLEN_VERY_LOW:
        return "very_low";
    case POLLEN_LOW:
        return "low";
    case POLLEN_MODERATE:
        return "moderate";
    case POLLEN_HIGH:
        return "high";
    default:
        return "very_high";
    }
}

enum Co2Quality
{
    CO2_EXCELLENT = 0,
    CO2_GOOD,
    CO2_FAIR,
    CO2_POOR,
    CO2_VERY_POOR
};

inline uint8_t classifyCo2Quality(uint16_t co2ppm)
{
    if (co2ppm < 800)
        return CO2_EXCELLENT;
    else if (co2ppm < 1000)
        return CO2_GOOD;
    else if (co2ppm < 1500)
        return CO2_FAIR;
    else if (co2ppm < 2000)
        return CO2_POOR;
    else
        return CO2_VERY_POOR;
}

inline const char *co2QualityName(uint8_t quality)
{
    switch (quality)
    {
    case CO2_EXCELLENT:
        return "excellent";
    case CO2_GOOD:
        return "good";
    case CO2_FAIR:
        return "fair";
    case CO2_POOR:
        return "poor";
    default:
        return "very_poor";
    }
}

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

inline const char *classifyPollenLevel(uint32_t pollenCount)
{
    if (pollenCount < 100)
        return "very_low";
    else if (pollenCount < 300)
        return "low";
    else if (pollenCount < 700)
        return "moderate";
    else if (pollenCount < 1200)
        return "high";
    else
        return "very_high";
}

inline const char *classifyCo2Quality(uint16_t co2ppm)
{
    if (co2ppm < 800)
        return "excellent";
    else if (co2ppm < 1000)
        return "good";
    else if (co2ppm < 1500)
        return "fair";
    else if (co2ppm < 2000)
        return "poor";
    else
        return "very_poor";
}

#pragma once
#include "OpcN3.h"

inline uint32_t calculatePollenCount(const OpcN3Data &data)
{
    uint32_t count = 0;
    for (int i = 16; i < 24; ++i) // bins 17-24 correspond to ~18-40 um
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

inline float calculatePollenConcentration(const OpcN3Data &data)
{
    uint32_t count = calculatePollenCount(data);
    float volume_m3 = (data.sample_flow_rate_ml_s * data.sampling_period_s) / 1e6f;
    if (volume_m3 <= 0.0f)
        return 0.0f;
    return count / volume_m3; // grains per cubic meter
}

inline uint8_t classifyPollenLevel(float grainsPerM3)
{
    if (grainsPerM3 < 10.0f)
        return POLLEN_VERY_LOW;
    else if (grainsPerM3 < 50.0f)
        return POLLEN_LOW;
    else if (grainsPerM3 < 200.0f)
        return POLLEN_MODERATE;
    else if (grainsPerM3 < 700.0f)
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

struct PollenAccumulator
{
    uint32_t totalCount = 0;
    float totalVolumeM3 = 0.0f;

    void addSample(const OpcN3Data &data)
    {
        totalCount += calculatePollenCount(data);
        totalVolumeM3 += (data.sample_flow_rate_ml_s * data.sampling_period_s) / 1e6f;
    }

    float concentration() const
    {
        return totalVolumeM3 > 0.0f ? totalCount / totalVolumeM3 : 0.0f;
    }

    void reset()
    {
        totalCount = 0;
        totalVolumeM3 = 0.0f;
    }
};

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

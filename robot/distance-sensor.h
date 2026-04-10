#ifndef DISTANCE_SENSOR_H
#define DISTANCE_SENSOR_H

#include "commands.h"

#include <SparkFun_VL53L5CX_Library.h>

class DistanceSensor {
public:
    using DistanceType = std::remove_extent_t<decltype(VL53L5CX_ResultsData::distance_mm)>;

    enum class Resolution : uint8_t {
        RES_4X4,
        RES_8X8,
    };

    bool begin(Resolution resolution = Resolution::RES_8X8);
    bool update();

    uint8_t countBlockedPixels(DistanceType threshold, uint8_t border = 0) const;

    const uint8_t* getSendBuffer() const { return m_distanceData; }
    size_t getSendBufferSize() const { return 1 + m_resolution * sizeof(DistanceType); }

    const DistanceType* getDistanceData() const { return reinterpret_cast<const DistanceType*>(m_distanceData + 1); }

    uint8_t getResolution() const { return m_resolution; }
    uint8_t getWidth() const { return m_width; }

private:
    SparkFun_VL53L5CX m_imager{};
    uint8_t m_distanceData[1 + sizeof(VL53L5CX_ResultsData::distance_mm)]{
        static_cast<uint8_t>(Command::distanceData)
    };
    uint8_t m_resolution{};
    uint8_t m_width{};
};

#endif // DISTANCE_SENSOR_H

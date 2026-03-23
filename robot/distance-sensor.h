#ifndef DISTANCE_SENSOR_H
#define DISTANCE_SENSOR_H

#include <SparkFun_VL53L5CX_Library.h>

class DistanceSensor {
public:
    enum class Resolution : uint8_t {
        RES_4X4,
        RES_8X8,
    };

    bool begin(Resolution resolution = Resolution::RES_8X8);
    bool update();

    const VL53L5CX_ResultsData& getLastMeasurement() const { return m_lastMeasurement; }
    uint8_t getResolution() const { return m_resolution; }
    uint8_t getWidth() const { return m_width; }

private:
    SparkFun_VL53L5CX m_imager{};
    VL53L5CX_ResultsData m_lastMeasurement{};
    uint8_t m_resolution{};
    uint8_t m_width{};
};

#endif // DISTANCE_SENSOR_H

#include "distance-sensor.h"
#include "config.h"

#include <Arduino.h>
#include <Wire.h>

bool DistanceSensor::begin(Resolution resolution) {
    pinMode(pins::RST, OUTPUT);
    digitalWrite(pins::RST, LOW);

    Wire.begin(pins::SDA, pins::SCL);
    Wire.setClock(400'000);

    Serial.println(F("Initializing distance sensor board. This can take up to 10s. Please wait."));
    if (!m_imager.begin()) {
        Serial.println(F("Error: Sensor not found - check your wiring."));
        return false;
    }

    switch (resolution) {
    case Resolution::RES_4X4:
        m_width = 4;
        break;
    case Resolution::RES_8X8:
        m_width = 8;
        break;
    default:
        Serial.println(F("Error: Invalid resolution selected."));
        return false;
    }

    m_resolution = m_width * m_width;
    m_imager.setResolution(m_resolution);

    m_imager.startRanging();
    m_imager.setRangingFrequency(15);
    return true;
}

bool DistanceSensor::update() {
    if (!m_imager.isDataReady()) {
        return false;
    }
    return m_imager.getRangingData(&m_lastMeasurement);
}

uint8_t DistanceSensor::countBlockedPixels(DistanceType threshold, uint8_t border) const {
    uint8_t count{};
    for (uint8_t y{ border }; y < m_width - border; ++y) {
        for (uint8_t x{ border }; x < m_width - border; ++x) {
            const uint8_t index{ y * m_width + x };
            if (m_lastMeasurement.distance_mm[index] <= threshold) {
                ++count;
            }
        }
    }
    return count;
}

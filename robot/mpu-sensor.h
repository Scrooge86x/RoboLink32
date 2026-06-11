#ifndef MPU_SENSOR_H
#define MPU_SENSOR_H

#include "commands.h"

#include <MPU6050_6Axis_MotionApps20.h>
#include <cstdint>

class MpuSensor {
public:
    bool begin(int interruptPin);
    void update();
    bool isReady() const;

    const uint8_t* getSendBuffer() const { return m_buffer; }
    size_t getSendBufferSize() const { return 1 + 3 * sizeof(float); }

private:
    MPU6050 m_mpu{};
    bool m_dmpReady{ false };
    volatile bool m_mpuInterrupt{ false };
    int m_interruptPin{ -1 };

    uint8_t m_buffer[1 + 3 * sizeof(float)]{ static_cast<uint8_t>(Command::mpuData) };
    float* const m_data{ reinterpret_cast<float*>(m_buffer + 1) };

    friend void onMpuInterrupt(void* arg);
};

#endif // MPU_SENSOR_H

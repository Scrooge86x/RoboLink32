#include "mpu-sensor.h"

#include "config.h"

#include <Arduino.h>
#include <Wire.h>

void IRAM_ATTR onMpuInterrupt(void* arg) {
    MpuSensor* sensor = static_cast<MpuSensor*>(arg);
    if (sensor) {
        sensor->m_mpuInterrupt = true;
    }
}

bool MpuSensor::begin(int interruptPin) {
    m_interruptPin = interruptPin;
    Wire.begin(pins::SDA, pins::SCL);
    Wire.setClock(400000);

    Serial.println("[MPU] Initializing...");
    m_mpu.initialize();
    if (!m_mpu.testConnection()) {
        Serial.println("[MPU] Connection failed");
        return false;
    }

    Serial.println("[MPU] Connection successful. Initializing DMP...");

    uint8_t devStatus = m_mpu.dmpInitialize();
    if (devStatus != 0) {
        Serial.printf("[MPU] DMP init failed (code %d)\n", devStatus);
        return false;
    }

    m_mpu.setXGyroOffset(0);
    m_mpu.setYGyroOffset(0);
    m_mpu.setZGyroOffset(0);
    m_mpu.CalibrateGyro(6);

    m_mpu.setXAccelOffset(0);
    m_mpu.setYAccelOffset(0);
    m_mpu.setZAccelOffset(0);
    m_mpu.CalibrateAccel(6);

    m_mpu.PrintActiveOffsets();

    m_mpu.setDMPEnabled(true);
    pinMode(m_interruptPin, INPUT_PULLUP);
    attachInterruptArg(digitalPinToInterrupt(m_interruptPin), onMpuInterrupt, this, RISING);
    m_mpu.getIntStatus();

    m_dmpReady = true;
    Serial.println("[MPU] DMP ready");
    return true;
}

void MpuSensor::update() {
    if (!m_dmpReady || !m_mpuInterrupt) {
        return;
    }

    m_mpuInterrupt = false;

    uint8_t fifoBuffer[64]{};
    Quaternion q{};
    VectorFloat gravity{};

    while (m_mpu.dmpGetCurrentFIFOPacket(fifoBuffer)) {
        m_mpu.dmpGetQuaternion(&q, fifoBuffer);
        m_mpu.dmpGetGravity(&gravity, &q);
        m_mpu.dmpGetYawPitchRoll(m_data, &q, &gravity);

        m_data[0] *= 180.0f / M_PI;
        m_data[1] *= 180.0f / M_PI;
        m_data[2] *= 180.0f / M_PI;
    }
}

bool MpuSensor::isReady() const {
    return m_dmpReady;
}

#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>
#include <type_traits>
#include <SparkFun_VL53L5CX_Library.h>

namespace pins {

constexpr uint8_t PWMB{ 0 };
constexpr uint8_t BIN2{ 1 };
constexpr uint8_t BIN1{ 2 };
constexpr uint8_t AIN1{ 3 };
constexpr uint8_t AIN2{ 4 };
constexpr uint8_t PWMA{ 5 };
constexpr uint8_t SCL{ 7 };
constexpr uint8_t SDA{ 8 };
constexpr uint8_t RST{ 9 };

} // pins

namespace buttons {
    constexpr uint8_t PAIR{ 9 };
} // buttons

namespace control {

constexpr uint16_t OBSTACLE_THRESHOLD_MM{ 150 };
constexpr uint8_t MAX_BLOCKED_PIXELS{ 12 };
constexpr int SLOW_SPEED{ 130 };
constexpr int FAST_SPEED{ 255 };
constexpr unsigned long TIMEOUT_MS{ 100 };

} // control

namespace pairing {

constexpr uint8_t broadcastChannel{ 6 };
constexpr uint16_t announceIntervalMs{ 1000 };
constexpr uint16_t timeoutMs{ 15000 };
constexpr uint16_t holdTimeMs{ 2500 };
constexpr const char robotName[]{ "RoboLink32" };
constexpr size_t maxNameLength{ 32 };

} // pairing

#endif // CONFIG_H

#ifndef CONFIG_H
#define CONFIG_H

namespace pins {

// Screen
constexpr uint8_t TFT_DC{ 33 };
constexpr uint8_t TFT_CS{ 34 };
constexpr uint8_t TFT_SCK{ 35 };
constexpr uint8_t TFT_MOSI{ 36 };
constexpr uint8_t TFT_RST{ 37 };
constexpr uint8_t TFT_BL{ 38 };

// Buttons
constexpr uint8_t KEY_X{ 2 };
constexpr uint8_t KEY_Y{ 5 };
constexpr uint8_t KEY_A{ 40 };
constexpr uint8_t KEY_B{ 41 };

// Joystick
constexpr uint8_t J_UP{ 13 };
constexpr uint8_t J_PRESS{ 14 };
constexpr uint8_t J_LEFT{ 42 };
constexpr uint8_t J_DOWN{ 1 };
constexpr uint8_t J_RIGHT{ 4 };

} // pins

namespace lcdscreen {

constexpr uint16_t WIDTH{ 240 };
constexpr uint16_t HEIGHT{ 240 };
constexpr uint8_t ROTATION{ 3 };

} // LCD Screen

#endif
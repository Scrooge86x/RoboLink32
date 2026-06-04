#ifndef CONFIG_H
#define CONFIG_H

#include "WiFi.h"
#include <TFT_eSPI.h>

namespace pins {

// Screen
constexpr uint8_t TFT_DC_PIN{ 33 };
constexpr uint8_t TFT_CS_PIN{ 34 };
constexpr uint8_t TFT_SCK_PIN{ 35 };
constexpr uint8_t TFT_MOSI_PIN{ 36 };
constexpr uint8_t TFT_RST_PIN{ 37 };
constexpr uint8_t TFT_BL_PIN{ 38 };

// Buttons
constexpr uint8_t KEY_X{ 2 };
constexpr uint8_t KEY_Y{ 5 };
constexpr uint8_t KEY_A{ 40 };
constexpr uint8_t KEY_B{ 41 };

// Joystick
constexpr uint8_t J_DOWN{ 1 };
constexpr uint8_t J_RIGHT{ 4 };
constexpr uint8_t J_UP{ 13 };
constexpr uint8_t J_PRESS{ 14 };
constexpr uint8_t J_LEFT{ 42 };

} // pins

namespace lcdscreen {

constexpr uint16_t WIDTH{ 240 };
constexpr uint16_t HEIGHT{ 240 };
constexpr uint8_t ROTATION{ 1 };

} // LCD Screen

namespace espnow {

constexpr wifi_mode_t WIFI_MODE{ WIFI_STA };

} // espnow

namespace message {

  constexpr uint8_t GRID_SIZE{ 8 };
  constexpr uint8_t PIXEL_BYTE_SIZE{ 2 };
  constexpr uint8_t TOTAL_BYTES = GRID_SIZE * GRID_SIZE * PIXEL_BYTE_SIZE;

}

namespace heatmap {

constexpr int16_t MIN_DISTANCE = 20;     // mm
constexpr int16_t MAX_DISTANCE = 800;    // mm
const uint16_t COLOR_BG = TFT_BLACK;

}

namespace pairing {

constexpr uint8_t BROADCAST_CHANNEL = 6;
constexpr uint8_t DEFAULT_CHANNEL   = 1;

}
#endif
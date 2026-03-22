#ifndef CONFIG_H
#define CONFIG_H

#include "WiFi.h"
#include "MacAddress.h"

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
constexpr uint8_t J_DOWN{ 1 };
constexpr uint8_t J_RIGHT{ 4 };
constexpr uint8_t J_UP{ 13 };
constexpr uint8_t J_PRESS{ 14 };
constexpr uint8_t J_LEFT{ 42 };

} // pins

namespace lcdscreen {

constexpr uint16_t WIDTH{ 240 };
constexpr uint16_t HEIGHT{ 240 };
constexpr uint8_t ROTATION{ 3 };

} // LCD Screen

namespace espnow {

constexpr int WIFI_CHANNEL{ 1 };
constexpr wifi_mode_t WIFI_MODE{ WIFI_STA };
constexpr wifi_interface_t WIFI_IF = WIFI_IF_STA;
const MacAddress PEER_MAC({0xdc, 0xda, 0x0c, 0xa1, 0x4f, 0xa4});

} // espnow

namespace message {

  constexpr char PULL_IMAGE_REQ{ '0' };
  constexpr uint8_t GRID_SIZE{ 8 };
  constexpr uint8_t PIXEL_BYTE_SIZE{ 2 };
  constexpr int TOTAL_BYTES = GRID_SIZE * GRID_SIZE * PIXEL_BYTE_SIZE;

  constexpr int GRAD_MIN{ 0 };
  constexpr int GRAD_MAX{ 400 };
  constexpr int CELL_SIZE{ 240 / GRID_SIZE };

}
#endif
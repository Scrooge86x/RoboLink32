#include "config.h"
#include "commands.h"
#include <TFT_eSPI.h>
#include <SPI.h>
#include <esp_now.h>
#include <WiFi.h>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

uint8_t rawDistanceData[message::TOTAL_BYTES]{};
const int16_t* distancePtr = nullptr;

constexpr int16_t MIN_DISTANCE{20};
constexpr int16_t MAX_DISTANCE{200};
const uint16_t COLOR_BG = TFT_BLACK;

unsigned long lastRequest = 0;

esp_now_peer_info_t peerInfo;

void printDistanceGrid() {

  Serial.println(F("=== Distance Grid ==="));

  for (uint8_t y = 0; y < message::GRID_SIZE; y++) {
        for (uint8_t x = 0; x < message::GRID_SIZE; x++) {
            uint8_t idx = y * message::GRID_SIZE + x;
            int16_t dist = distancePtr[idx];

            Serial.print(dist);
            Serial.print(F("  "));
        }
        Serial.println();
    }
  
  Serial.println(F("====================="));
}

void sendCommand(Command cmd) {
  uint8_t data = static_cast<uint8_t>(cmd);
  esp_now_send(espnow::peerAddress, &data, 1);
}

void requestDataFromRobot() {
  if (millis() - lastRequest > 100) {
    sendCommand(Command::requestDistanceData);
    lastRequest = millis();
  }
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len < 1) {
    return;
  }

  switch (static_cast<Command>(incomingData[0])) {
    case Command::distanceData:
      std::copy(incomingData + 1, incomingData + message::TOTAL_BYTES, rawDistanceData);
      distancePtr = reinterpret_cast<const int16_t*>(rawDistanceData);
      printDistanceGrid();
      draw();
    break;
  }
}

uint16_t hslToColor565(float h, float s, float l) {
    float r, g, b;

    if (s == 0) {
        r = g = b = l;
    } else {
        auto hue2rgb = [](float p, float q, float t) -> float {
            if (t < 0) t += 1;
            if (t > 1) t -= 1;
            if (t < 1.0f/6) return p + (q - p) * 6 * t;
            if (t < 1.0f/2) return q;
            if (t < 2.0f/3) return p + (q - p) * (2.0f/3 - t) * 6;
            return p;
        };

        float q = l < 0.5f ? l * (1 + s) : l + s - l * s;
        float p = 2 * l - q;

        r = hue2rgb(p, q, h + 1.0f/3);
        g = hue2rgb(p, q, h);
        b = hue2rgb(p, q, h - 1.0f/3);
    }

    uint8_t red   = (uint8_t)(r * 255);
    uint8_t green = (uint8_t)(g * 255);
    uint8_t blue  = (uint8_t)(b * 255);

    return spr.color565(red, green, blue);
}

uint16_t getHeatmapColor(uint16_t dist) {
  if (dist < 0) {
        return TFT_BLACK;
    }

  int16_t clamped = constrain(dist, MIN_DISTANCE, MAX_DISTANCE);

  uint8_t hue = map(clamped, MIN_DISTANCE, MAX_DISTANCE, 0, 240);

  return hslToColor565(hue / 360.0f, 1.0f, 0.5f);
}

void draw() {
  spr.fillSprite(COLOR_BG);

  constexpr uint8_t grid       = message::GRID_SIZE;
  constexpr uint16_t cellWidth  = lcdscreen::WIDTH  / grid;
  constexpr uint16_t cellHeight = lcdscreen::HEIGHT / grid;

  for (uint8_t y = 0; y < grid; y++) {
    for (uint8_t x = 0; x < grid; x++) {
      int16_t dist = distancePtr[y * grid + x];

      uint16_t color = getHeatmapColor(dist);

      spr.fillRect(x * cellWidth, y * cellHeight, cellWidth, cellHeight, color);
    }
  }

  spr.pushSprite(0, 0);
}

void handleMovement() {
  static unsigned long lastSend = 0;
  const unsigned long interval = 120;

  if (millis() - lastSend < interval) {
    return;
  }

  bool fastMode = (digitalRead(pins::KEY_A) == LOW);

  Command cmd{};

  if (digitalRead(pins::J_UP) == LOW) {
    cmd = fastMode ? Command::forwardFast : Command::forwardSlow;
  }
  else if (digitalRead(pins::J_DOWN) == LOW) {
    cmd = fastMode ? Command::backwardFast : Command::backwardSlow;
  }
  else if (digitalRead(pins::J_LEFT) == LOW) {
    cmd = fastMode ? Command::leftFast : Command::leftSlow;
  }
  else if (digitalRead(pins::J_RIGHT) == LOW) {
    cmd = fastMode ? Command::rightFast : Command::rightSlow;
  }

  if (cmd != Command{}) {
    sendCommand(cmd);
    lastSend = millis();
  }
}

void setupScreen() {
  pinMode(pins::TFT_BL, OUTPUT);
  digitalWrite(pins::TFT_BL, HIGH);
  tft.init();
  tft.setRotation(lcdscreen::ROTATION);
  if (!spr.createSprite(lcdscreen::WIDTH, lcdscreen::HEIGHT)) {
    Serial.println("Sprite initialization failed!");
  }
  tft.fillScreen(TFT_RED);
}

void setupKeys() {
  pinMode(pins::KEY_A, INPUT_PULLUP);
  pinMode(pins::KEY_B, INPUT_PULLUP);
  pinMode(pins::KEY_X, INPUT_PULLUP);
  pinMode(pins::KEY_Y, INPUT_PULLUP);
}

void setupJoystick() {
  pinMode(pins::J_UP,    INPUT_PULLUP);
  pinMode(pins::J_DOWN,  INPUT_PULLUP);
  pinMode(pins::J_LEFT,  INPUT_PULLUP);
  pinMode(pins::J_RIGHT, INPUT_PULLUP);
  pinMode(pins::J_PRESS, INPUT_PULLUP);
}

void setupESPNow() {
  WiFi.mode(espnow::WIFI_MODE);
  WiFi.setChannel(espnow::WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  memcpy(peerInfo.peer_addr, espnow::peerAddress, 6);
  peerInfo.channel = espnow::WIFI_CHANNEL;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
}

void setup() {
  Serial.begin(115200);
  setupScreen();
  setupKeys();
  setupJoystick();
  setupESPNow();
  Serial.println("Initialization completed.");
}

void loop() {
  requestDataFromRobot();
}
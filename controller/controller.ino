#include "config.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

#include "ESP32_NOW_Serial.h"

Adafruit_ST7789 tft = Adafruit_ST7789(pins::TFT_CS, pins::TFT_DC, pins::TFT_MOSI, pins::TFT_SCK, pins::TFT_RST);

ESP_NOW_Serial_Class NowSerial(espnow::PEER_MAC, espnow::WIFI_CHANNEL, espnow::WIFI_IF);

int16_t distanceData[message::GRID_SIZE][message::GRID_SIZE];
int16_t smoothed[message::GRID_SIZE][message::GRID_SIZE];

unsigned long lastRequest = 0;

void draw() {
  for (int y = 0; y < message::GRID_SIZE; y++) {
    for (int x = 0; x < message::GRID_SIZE; x++) {

      int16_t value = smoothed[y][x];
      uint16_t color = getHeatmapColor(value);

      int px = x * message::CELL_SIZE;
      int py = y * message::CELL_SIZE;

      tft.fillRect(px, py, message::CELL_SIZE, message::CELL_SIZE, color);
    }
  }
}

void computeSmooth() {
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      smoothed[y][x] = getSmoothedValue(x, y);
    }
  }
}

int16_t getSmoothedValue(int x, int y) {
  int16_t sum = 0;
  uint16_t count = 0;

  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      int nx = x + dx;
      int ny = y + dy;

      if (nx >= 0 && ny >= 0 &&
          nx < message::GRID_SIZE &&
          ny < message::GRID_SIZE) {
        sum += distanceData[ny][nx];
        count++;
      }
    }
  }

  return sum / count;
}

uint16_t getHeatmapColor(int16_t value) {
  value = constrain(value, message::GRAD_MIN, message::GRAD_MAX);
  uint8_t x = map(value, message::GRAD_MIN, message::GRAD_MAX, 0, 255);

  uint8_t r, g, b;

  if (x < 85) {
    r = x * 3;
    g = 0;
    b = 128 - (x * 1.5);
  } else if (x < 170) {
    r = 255;
    g = (x - 85) * 3;
    b = 0;
  } else {
    r = 255;
    g = 255;
    b = (x - 170) * 3;
  }

  return tft.color565(r, g, b);
}

void requestDataFromRobot() {
  if (millis() - lastRequest > 100) {
    NowSerial.write(message::PULL_IMAGE_REQ);
    lastRequest = millis();
  }
}

void receiveDataFromRobot() {
  if (NowSerial.available() >= message::TOTAL_BYTES) { 
    NowSerial.readBytes((uint8_t*)distanceData, message::TOTAL_BYTES);

    computeSmooth();
    draw();
  }
}

void setupScreen() {
  pinMode(pins::TFT_BL, OUTPUT);
  digitalWrite(pins::TFT_BL, HIGH);

  tft.init(lcdscreen::WIDTH, lcdscreen::HEIGHT);
  tft.setRotation(lcdscreen::ROTATION);
}

void setupKeys() {
  pinMode(pins::KEY_A, INPUT_PULLUP);
  pinMode(pins::KEY_B, INPUT_PULLUP);
  pinMode(pins::KEY_X, INPUT_PULLUP);
  pinMode(pins::KEY_Y, INPUT_PULLUP);
}

void setupWiFi() {
  WiFi.mode(espnow::WIFI_MODE);
  WiFi.setChannel(espnow::WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  while (!WiFi.STA.started()) {
    delay(100);
  }

  NowSerial.begin(115200);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  setupScreen();
  setupKeys();

  Serial.println("Initialization completed.");
}

void loop() {
  // put your main code here, to run repeatedly:
  requestDataFromRobot();
  
  receiveDataFromRobot();

  handleKeyA();
}

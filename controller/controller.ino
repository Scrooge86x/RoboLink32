#include "config.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

Adafruit_ST7789 tft = Adafruit_ST7789(pins::TFT_CS, pins::TFT_DC, pins::TFT_MOSI, pins::TFT_SCK, pins::TFT_RST);

void draw() {
  tft.fillScreen(ST77XX_RED);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Hello");
  Serial.println("Koniec draw()");
}

void setupScreen() {
  pinMode(pins::TFT_BL, OUTPUT);
  digitalWrite(pins::TFT_BL, HIGH);

  tft.init(lcdscreen::WIDTH, lcdscreen::HEIGHT);
  tft.setRotation(lcdscreen::ROTATION);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  setupScreen();

  draw();
  Serial.println("Initialization completed.");
}

void loop() {
  // put your main code here, to run repeatedly:

  
}

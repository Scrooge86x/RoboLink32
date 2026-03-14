#include "config.h"
#include "motor.h"

#include <WiFi.h>
#include <NetworkClient.h>
#include <WiFiAP.h>
#include <AsyncUDP.h>

#include <Wire.h>
#include <SparkFun_VL53L5CX_Library.h>

#include <type_traits>

const char* ssid{ "secret-wifi-network" };
const char* password{ "p@ssw0rd" };

AsyncUDP udp;

SparkFun_VL53L5CX myImager;
VL53L5CX_ResultsData measurementData;

uint8_t imageResolution{};
uint8_t imageWidth{};

bool g_forwardEnabled{};
unsigned long g_lastControlTime{};

void handleInput(const char input) {
  switch (input) {
    case 'w':
      if (!g_forwardEnabled) {
        return;
      }
      motor::forward(control::SLOW_SPEED);
      break;
    case 'W':
      if (!g_forwardEnabled) {
        return;
      }
      motor::forward(control::FAST_SPEED);
      break;
    case 's': motor::reverse(control::SLOW_SPEED); break;
    case 'S': motor::reverse(control::FAST_SPEED); break;
    case 'a': motor::left(control::SLOW_SPEED);    break;
    case 'A': motor::left(control::FAST_SPEED);    break;
    case 'd': motor::right(control::SLOW_SPEED);   break;
    case 'D': motor::right(control::FAST_SPEED);   break;
    default: break;
  }

  g_lastControlTime = millis();
}

void setupUdpServer() {
  Serial.println();
  Serial.println("Configuring access point...");

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(ssid, password)) {
    log_e("Soft AP creation failed.");
    while (1)
      ;
  }

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  Serial.println("Server started");

  if (udp.listen(1234)) {
    udp.onPacket([](AsyncUDPPacket packet) {
      switch (packet.data()[0]) {
        case '0':
          if (myImager.isDataReady() != true) {
            return;
          }
          if (myImager.getRangingData(&measurementData)) {
            packet.write(
              reinterpret_cast<uint8_t*>(measurementData.distance_mm),
              imageResolution * sizeof(sensor::DistanceType)
            );
          }
          break;
        case '1':
          Serial.printf("%c", packet.data()[1]);
          handleInput(packet.data()[1]);
          break;
      }
    });
  }
}

void setupSensor() {
  pinMode(pins::RST, OUTPUT);
  digitalWrite(pins::RST, LOW);

  Wire.begin(pins::SDA, pins::SCL);
  Wire.setClock(400000);

  Serial.println("Initializing sensor board. This can take up to 10s. Please wait.");
  if (myImager.begin() == false) {
    Serial.println(F("Sensor not found - check your wiring. Freezing"));
    while (1)
      ;
  }

  myImager.setResolution(8 * 8);

  imageResolution = myImager.getResolution();
  imageWidth = sqrt(imageResolution);

  myImager.startRanging();
  myImager.setRangingFrequency(15);
}

void setup() {
  Serial.begin(115200);
  setupSensor();
  motor::setup();
  setupUdpServer();
}

void loop() {
  if (millis() - g_lastControlTime > control::TIMEOUT_MS) {
    motor::stop();
  }

  uint8_t count{};
  for (uint8_t i{}; i < imageResolution; ++i) {
    if (measurementData.distance_mm[i] <= control::OBSTACLE_THRESHOLD_MM) {
      ++count;
    }
  }
  g_forwardEnabled = count <= control::MAX_BLOCKED_PIXELS;

  delay(50);
}

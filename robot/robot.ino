#include "config.h"
#include "distance-sensor.h"
#include "motor.h"

#include <AsyncUDP.h>
#include <WiFi.h>
#include <Wire.h>

const char* ssid{ "secret-wifi-network" };
const char* password{ "p@ssw0rd" };

AsyncUDP udp;
DistanceSensor g_distanceSensor{};

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
        case '0': {
          const auto& measurement{ g_distanceSensor.getLastMeasurement() };
          packet.write(
            reinterpret_cast<const uint8_t*>(measurement.distance_mm),
            g_distanceSensor.getResolution() * sizeof(distanceSensor::DistanceType)
          );
        } break;
        case '1':
          Serial.printf("%c", packet.data()[1]);
          handleInput(packet.data()[1]);
          break;
      }
    });
  }
}

void setup() {
  Serial.begin(115200);

  if (!g_distanceSensor.begin()) {
      while (1)
        ;
  }

  motor::setup();
  setupUdpServer();
}

void loop() {
  if (millis() - g_lastControlTime > control::TIMEOUT_MS) {
    motor::stop();
  }

  g_distanceSensor.update();
  const auto& measurement{ g_distanceSensor.getLastMeasurement() };

  uint8_t count{};
  for (uint8_t i{}; i < g_distanceSensor.getResolution(); ++i) {
    if (measurement.distance_mm[i] <= control::OBSTACLE_THRESHOLD_MM) {
      ++count;
    }
  }
  g_forwardEnabled = count <= control::MAX_BLOCKED_PIXELS;

  delay(50);
}

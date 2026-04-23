#include "config.h"
#include "distance-sensor.h"
#include "motor.h"

#include <esp_now.h>
#include <WiFi.h>

DistanceSensor g_distanceSensor{};

bool g_forwardEnabled{};
unsigned long g_lastControlTime{};

void onEspNowDataRecv(const esp_now_recv_info_t* espNowInfo, const uint8_t* data, int length) {
  if (length < 1) {
    return;
  }

  Serial.println(data[0]);

  switch (static_cast<Command>(data[0])) {
    case Command::forwardSlow:
      if (!g_forwardEnabled) {
        return;
      }
      motor::forward(control::SLOW_SPEED); 
      break;
    case Command::forwardFast:
      if (!g_forwardEnabled) {
        return;
      }
      motor::forward(control::FAST_SPEED); 
      break;
    case Command::backwardSlow: motor::reverse(control::SLOW_SPEED); break;
    case Command::backwardFast: motor::reverse(control::FAST_SPEED); break;
    case Command::leftSlow:     motor::left(control::SLOW_SPEED);    break;
    case Command::leftFast:     motor::left(control::FAST_SPEED);    break;
    case Command::rightSlow:    motor::right(control::SLOW_SPEED);   break;
    case Command::rightFast:    motor::right(control::FAST_SPEED);   break;
    case Command::requestDistanceData:
      esp_now_send(
        connection::peerAddress,
        g_distanceSensor.getSendBuffer(),
        g_distanceSensor.getSendBufferSize()
      );
      return;
  }

  g_lastControlTime = millis();
}

void setup() {
  Serial.begin(115200);

  if (!g_distanceSensor.begin()) {
    while (1)
      ;
  }

  motor::setup();

  WiFi.mode(WIFI_STA);
  WiFi.setChannel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println(F("Error initializing ESP-NOW."));
    while (1)
      ;
  }

  esp_now_peer_info_t peerInfo{};
  peerInfo.channel = connection::channel;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;
  memcpy(peerInfo.peer_addr, connection::peerAddress, 6);

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println(F("Failed to add ESP-NOW peer."));
    while (1)
      ;
  }

  esp_now_register_recv_cb(onEspNowDataRecv);
}

void loop() {
  if (millis() - g_lastControlTime > control::TIMEOUT_MS) {
    motor::stop();
  }

  g_distanceSensor.update();

  uint8_t blockedPixels{ g_distanceSensor.countBlockedPixels(control::OBSTACLE_THRESHOLD_MM, 1) };
  g_forwardEnabled = blockedPixels <= control::MAX_BLOCKED_PIXELS;

  delay(50);
}

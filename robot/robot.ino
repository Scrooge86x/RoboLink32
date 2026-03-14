#include <WiFi.h>
#include <NetworkClient.h>
#include <WiFiAP.h>
#include <AsyncUDP.h>

#include <Wire.h>
#include <SparkFun_VL53L5CX_Library.h>

#include <type_traits>

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

const char* ssid{ "secret-wifi-network" };
const char* password{ "p@ssw0rd" };

AsyncUDP udp;

SparkFun_VL53L5CX myImager;
VL53L5CX_ResultsData measurementData;

using DistanceType = std::remove_extent_t<decltype(VL53L5CX_ResultsData::distance_mm)>;

uint8_t imageResolution{};
uint8_t imageWidth{};

namespace motor {

void setup() {
  pinMode(pins::AIN1, OUTPUT);
  pinMode(pins::AIN2, OUTPUT);
  pinMode(pins::PWMA, OUTPUT);

  pinMode(pins::BIN1, OUTPUT);
  pinMode(pins::BIN2, OUTPUT);
  pinMode(pins::PWMB, OUTPUT);
}

void reverse(const int speed) {
  digitalWrite(pins::AIN1, HIGH);
  digitalWrite(pins::AIN2, LOW);
  analogWrite(pins::PWMA, speed);

  digitalWrite(pins::BIN1, LOW);
  digitalWrite(pins::BIN2, HIGH);
  analogWrite(pins::PWMB, speed);
}

void forward(const int speed) {
  digitalWrite(pins::AIN1, LOW);
  digitalWrite(pins::AIN2, HIGH);
  analogWrite(pins::PWMA, speed);

  digitalWrite(pins::BIN1, HIGH);
  digitalWrite(pins::BIN2, LOW);
  analogWrite(pins::PWMB, speed);
}

void right(const int speed) {
  digitalWrite(pins::AIN1, LOW);
  digitalWrite(pins::AIN2, HIGH);
  analogWrite(pins::PWMA, speed);

  digitalWrite(pins::BIN1, LOW);
  digitalWrite(pins::BIN2, HIGH);
  analogWrite(pins::PWMB, speed);
}

void left(const int speed) {
  digitalWrite(pins::AIN1, HIGH);
  digitalWrite(pins::AIN2, LOW);
  analogWrite(pins::PWMA, speed);

  digitalWrite(pins::BIN1, HIGH);
  digitalWrite(pins::BIN2, LOW);
  analogWrite(pins::PWMB, speed);
}

void stop() {
  digitalWrite(pins::AIN1, LOW);
  digitalWrite(pins::AIN2, LOW);
  analogWrite(pins::PWMA, 255);

  digitalWrite(pins::BIN1, LOW);
  digitalWrite(pins::BIN2, LOW);
  analogWrite(pins::PWMB, 255);
}

} // motor

bool g_forwardEnabled{};
unsigned long g_lastControlTime{};
constexpr unsigned long CONTROL_TIMEOUT_MS{ 100 };
constexpr DistanceType OBSTACLE_THRESHOLD_MM{ 100 };
constexpr uint8_t MAX_BLOCKED_PIXELS{ 12 };

void handleInput(const char input) {
  constexpr int slowSpeed{ 130 };
  constexpr int fastSpeed{ 255 };

  switch (input) {
    case 'w':
      if (!g_forwardEnabled) {
        return;
      }
      motor::forward(slowSpeed);
      break;
    case 'W':
      if (!g_forwardEnabled) {
        return;
      }
      motor::forward(fastSpeed);
      break;
    case 's': motor::reverse(slowSpeed); break;
    case 'S': motor::reverse(fastSpeed); break;
    case 'a': motor::left(slowSpeed); break;
    case 'A': motor::left(fastSpeed); break;
    case 'd': motor::right(slowSpeed); break;
    case 'D': motor::right(fastSpeed); break;
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
            constexpr auto dataSize{ sizeof(DistanceType) };
            packet.write(reinterpret_cast<uint8_t*>(measurementData.distance_mm), imageResolution * dataSize);
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
  if (millis() - g_lastControlTime > CONTROL_TIMEOUT_MS) {
    motor::stop();
  }

  uint8_t count{};
  for (uint8_t i{}; i < imageResolution; ++i) {
    if (measurementData.distance_mm[i] <= OBSTACLE_THRESHOLD_MM) {
      ++count;
    }
  }
  g_forwardEnabled = count <= MAX_BLOCKED_PIXELS;

  delay(50);
}

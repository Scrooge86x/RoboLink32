#include "config.h"
#include "distance-sensor.h"
#include "motor.h"
#include "pairing.h"

#include <esp_now.h>
#include <WiFi.h>

DistanceSensor g_distanceSensor{};

bool g_forwardEnabled{};
unsigned long g_lastControlTime{};

void onEspNowDataRecv(const esp_now_recv_info_t* espNowInfo, const uint8_t* data, int length) {
    if (length < 1) {
        return;
    }

    if (pairing::isActive()) {
        pairing::onDataRecv(espNowInfo->src_addr, data, length);
        return;
    }

    if (!pairing::isPaired()) {
        return;
    }
    if (memcmp(espNowInfo->src_addr, pairing::getPairedMac(), 6) != 0) {
        return;
    }

    switch (static_cast<Command>(data[0])) {
        case Command::backwardSlow: motor::reverse(control::SLOW_SPEED); break;
        case Command::backwardFast: motor::reverse(control::FAST_SPEED); break;
        case Command::leftSlow:     motor::left(control::SLOW_SPEED);    break;
        case Command::leftFast:     motor::left(control::FAST_SPEED);    break;
        case Command::rightSlow:    motor::right(control::SLOW_SPEED);   break;
        case Command::rightFast:    motor::right(control::FAST_SPEED);   break;
        case Command::forwardSlow:
            if (g_forwardEnabled) {
                motor::forward(control::SLOW_SPEED);
            }
            break;
        case Command::forwardFast:
            if (g_forwardEnabled) {
                motor::forward(control::FAST_SPEED);
            }
            break;
        case Command::requestDistanceData:
            esp_now_send(
                pairing::getPairedMac(),
                g_distanceSensor.getSendBuffer(),
                g_distanceSensor.getSendBufferSize()
            );
            return;
        default:
            return;
    }

    g_lastControlTime = millis();
}

void setup() {
    randomSeed(esp_random());
    Serial.begin(115200);
    pinMode(buttons::PAIR, INPUT_PULLUP);

    if (!g_distanceSensor.begin()) {
        while (1)
          ;
    }

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println(F("Error initializing ESP-NOW."));
        while (1)
            ;
    }

    esp_now_register_recv_cb(onEspNowDataRecv);

    motor::setup();
    pairing::setup();
}

static bool readPairButton() {
    pinMode(buttons::PAIR, INPUT_PULLUP);
    delayMicroseconds(10);
    bool pressed = (digitalRead(buttons::PAIR) == LOW);
    pinMode(buttons::PAIR, OUTPUT);
    digitalWrite(buttons::PAIR, LOW);
    return pressed;
}

void loop() {
    static unsigned long buttonPressStart{};
    static bool buttonWasPressed{};
    static bool holdTriggered{}; 

    pairing::update();

    if (pairing::isActive()) {
        delay(10);
        return;
    }

    bool nowPressed = readPairButton();
    if (nowPressed && !buttonWasPressed) {
        buttonPressStart = millis();
        holdTriggered = false;
    }
    if (nowPressed && !holdTriggered && (millis() - buttonPressStart >= pairing::holdTimeMs)) {
        pairing::start();
        holdTriggered = true;
    }
    if (!nowPressed) {
        buttonWasPressed = false;
        holdTriggered = false;
    }
    buttonWasPressed = nowPressed;

    if (millis() - g_lastControlTime > control::TIMEOUT_MS) {
        motor::stop();
    }

    g_distanceSensor.update();

    uint8_t blockedPixels{ g_distanceSensor.countBlockedPixels(control::OBSTACLE_THRESHOLD_MM, 1) };
    g_forwardEnabled = blockedPixels <= control::MAX_BLOCKED_PIXELS;

    delay(50);
}

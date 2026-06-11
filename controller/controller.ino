#include "config.h"
#include "commands.h"
#include "display.h"
#include "pairing.h"
#include "controls.h"
#include "comms.h"
#include <TFT_eSPI.h>
#include <SPI.h>
#include <esp_now.h>
#include <WiFi.h>

void setup() {
    Serial.begin(115200);
    initDisplay();
    setupKeys();
    setupJoystick();
    setupESPNow();
    initPairing();
    drawNotPaired();
}

void loop() {
    if (millis() - lastSeen > message::TIMEOUT_MS && !isPairingMode()) {
        drawNotPaired();
    }

    updatePairing();

    if (isPairingMode()) {
        handlePairingInput();
    } else {
        handleMovement();
        requestDataFromRobot();
        requestMpuDataFromRobot();
    }

    static uint32_t lastMpuToggle{};
    if (digitalRead(pins::KEY_B) == LOW && digitalRead(pins::KEY_Y) == HIGH) {
        if (millis() - lastMpuToggle > debounce::ANTI_DEBOUNCE_TIMEOUT) {
            if (!isPairingMode() && isPaired()) {
                displayMpu = !displayMpu;
                Serial.print("[MPU] Display toggled: ");
                Serial.println(displayMpu ? "ON" : "OFF");
            }
        }
        lastMpuToggle = millis();
    }

    static uint32_t lastMenuPress{};
    if (digitalRead(pins::KEY_Y) == LOW && digitalRead(pins::KEY_B) == LOW) {
        if (millis() - lastMenuPress > debounce::ANTI_DEBOUNCE_TIMEOUT) {
            if (isPairingMode()) {
                exitPairingMode();
            } else {
                enterPairingMode();
            }
            lastMenuPress = millis();
        }
    }
}
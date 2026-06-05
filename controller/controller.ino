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
    }

    static unsigned long lastMenuPress = 0;
    if (digitalRead(pins::KEY_Y) == LOW && digitalRead(pins::KEY_B) == LOW) {
        if (millis() - lastMenuPress > 400) {
            if (isPairingMode()) {
                exitPairingMode();
            } else {
                enterPairingMode();
            }
            lastMenuPress = millis();
        }
    }
}
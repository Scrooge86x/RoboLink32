#include "controls.h"
#include "config.h"
#include "commands.h"
#include "comms.h"
#include "pairing.h"
#include <Arduino.h>

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

void handleMovement() {
    static unsigned long lastSend = 0;
    const unsigned long interval = 120;
    if (!isPaired()) return;
    if (millis() - lastSend < interval) return;
    bool fastMode = (digitalRead(pins::KEY_A) == LOW);
    if (digitalRead(pins::J_UP) == LOW) {
        sendCommand(fastMode ? Command::forwardFast : Command::forwardSlow);
    } else if (digitalRead(pins::J_DOWN) == LOW) {
        sendCommand(fastMode ? Command::backwardFast : Command::backwardSlow);
    } else if (digitalRead(pins::J_LEFT) == LOW) {
        sendCommand(fastMode ? Command::leftFast : Command::leftSlow);
    } else if (digitalRead(pins::J_RIGHT) == LOW) {
        sendCommand(fastMode ? Command::rightFast : Command::rightSlow);
    } else {
        return;
    }
    lastSend = millis();
}
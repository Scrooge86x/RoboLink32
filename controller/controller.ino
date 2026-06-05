#include "config.h"
#include "commands.h"
#include "display.h"
#include "pairing.h"
#include <TFT_eSPI.h>
#include <SPI.h>
#include <esp_now.h>
#include <WiFi.h>

uint16_t distanceData[message::GRID_SIZE][message::GRID_SIZE]{};

unsigned long lastRequest{};
unsigned long lastSeen{ millis() };

void sendCommand(Command cmd) {
    sendCommandToPaired(cmd);
}

void sendCommandTo(const uint8_t* mac, Command cmd) {
    uint8_t data = static_cast<uint8_t>(cmd);
    Serial.printf("[SEND] Sending command 0x%02X to ", cmd);
    printMac(mac); 
    Serial.println();
    esp_now_send(mac, &data, 1);
}

void requestDataFromRobot() {
    if (isPairingMode()) return;
    if (!isPaired()) return;
    if (millis() - lastRequest > 100) {
        sendCommand(Command::requestDistanceData);
        lastRequest = millis();
    }
}

void transformDistanceData(uint16_t src[message::GRID_SIZE][message::GRID_SIZE]) {
    uint16_t temp[message::GRID_SIZE][message::GRID_SIZE];
    memcpy(temp, src, message::TOTAL_BYTES);
    for (uint8_t y = 0; y < message::GRID_SIZE; y++) {
        for (uint8_t x = 0; x < message::GRID_SIZE; x++) {
            distanceData[y][x] = temp[message::GRID_SIZE - 1 - x][y];
        }
    }
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
    Serial.print("[RX] from ");
    printMac(info->src_addr);
    Serial.printf(", len=%d, cmd=0x%02X\n", len, len>0 ? incomingData[0] : 0);

    if (len < 1) return;

    Command cmd = static_cast<Command>(incomingData[0]);
    if (cmd == Command::broadcastPairing || cmd == Command::pairSuccess) {
        handlePairingReceive(info->src_addr, incomingData, len);
        return;
    }

    if (!isPaired() || memcmp(info->src_addr, getPairedMac(), 6) != 0) {
        Serial.println("[RX] Ignoring data from non-paired robot");
        return;
    }

    switch (cmd) {
        case Command::distanceData:
            {
                lastSeen = millis();
                uint16_t raw[message::GRID_SIZE][message::GRID_SIZE];
                memcpy(raw, incomingData + 1, message::TOTAL_BYTES);
                transformDistanceData(raw);
                if (!isPairingMode()) {
                    drawHeatmap(distanceData);
                }
                Serial.println("[RX] Distance data received and displayed");
            }
            break;

        default:
            Serial.printf("[RX] Unknown command 0x%02X ignored\n", incomingData[0]);
            break;
    }
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

void setupESPNow() {
    WiFi.mode(WIFI_STA);
    WiFi.setChannel(pairing::DEFAULT_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR] esp_now_init failed");
        return;
    }
    esp_now_register_recv_cb(OnDataRecv);
    Serial.println("[ESP-NOW] Initialized, recv callback registered");

    esp_now_peer_info_t broadcastPeer = {};
    memset(broadcastPeer.peer_addr, 0xFF, 6);
    broadcastPeer.channel = 1;
    broadcastPeer.encrypt = false;
    broadcastPeer.ifidx = WIFI_IF_STA;
    if (esp_now_add_peer(&broadcastPeer) != ESP_OK) {
        Serial.println("[PEER] Failed to add broadcast peer");
    } else {
        Serial.println("[PEER] Broadcast peer (FF:FF:FF:FF:FF:FF) added");
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n[BOOT] Controller starting");
    initDisplay();
    setupKeys();
    setupJoystick();
    setupESPNow();
    initPairing();
    drawNotPaired();
    Serial.println("[BOOT] Ready");
}

void loop() {
    if (millis() - lastSeen > message::TIMEOUT_MS ) {
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
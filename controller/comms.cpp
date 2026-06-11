#include "esp32-hal.h"
#include "comms.h"
#include "config.h"
#include "display.h"
#include "pairing.h"
#include <SPI.h>
#include <esp_now.h>
#include <WiFi.h>
#include <cstring>

uint16_t distanceData[message::GRID_SIZE][message::GRID_SIZE]{};
unsigned long lastRequest{};
unsigned long lastSeen{ millis() };

bool displayMpu{ false };
float yaw{ 0.0f }, pitch{ 0.0f }, roll{ 0.0f };
uint32_t lastMpuRequest{};

extern void printMac(const uint8_t* mac);

void sendCommand(Command cmd) {
    sendCommandTo(getPairedMac(), cmd);
}

void sendCommandTo(const uint8_t* mac, Command cmd) {
    uint8_t data = static_cast<uint8_t>(cmd);
    Serial.printf("[SEND] Sending command 0x%02X to ", cmd);
    printMac(mac);
    Serial.println();
    esp_now_send(mac, &data, 1);
}

void requestDataFromRobot() {
    if (isPairingMode() || !isPaired()) {
        return;
    }
    if (millis() - lastRequest < message::DISTANCE_REQUEST_INTERVAL) {
        return;
    }
    sendCommand(Command::requestDistanceData);
    lastRequest = millis();
}

void requestMpuDataFromRobot() {
    if (isPairingMode() || !isPaired() || !displayMpu) {
        return;
    }
    if (millis() - lastMpuRequest < message::MPU_REQUEST_INTERVAL) {
        return;
    }
    sendCommand(Command::requestMpuData);
    lastMpuRequest = millis();
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
    Serial.printf(", len=%d, cmd=0x%02X\n", len, len > 0 ? incomingData[0] : 0);

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
        break;
        }
        case Command::mpuData:
        {
            if (len < 1 + 3 * sizeof(float)) {
                break;
            }

            memcpy(&yaw, incomingData + 1, sizeof(float));
            memcpy(&pitch, incomingData + 1 + sizeof(float), sizeof(float));
            memcpy(&roll, incomingData + 1 + 2*sizeof(float), sizeof(float));
            Serial.println("[RX] MPU data received and displayed");
        }

        default:
            Serial.printf("[RX] Unknown command 0x%02X ignored\n", incomingData[0]);
            break;
    }
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
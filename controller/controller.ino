#include "config.h"
#include "commands.h"
#include "display.h"
#include <TFT_eSPI.h>
#include <SPI.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Preferences.h>
#include <vector>
#include <algorithm>
#include <cstring>

// Globalna tablica odległości (mapa ciepła)
uint16_t distanceData[message::GRID_SIZE][message::GRID_SIZE]{};

unsigned long lastRequest = 0;

// ====================== PAROWANIE ======================
std::vector<DiscoveredRobot> discoveredRobots;
int selectedIndex = 0;

enum class Screen {
    Main,
    PairingMenu
};

Screen currentScreen = Screen::Main;

Preferences prefs;

bool hasPairedRobot = false;
uint8_t pairedMac[6];
uint8_t pairedChannel = pairing::DEFAULT_CHANNEL;

bool waitingForPairResponse = false;
uint8_t pendingPairMac[6];
unsigned long pairingRequestTime = 0;

// ====================== POMOCNICZE ======================
void printMac(const uint8_t* mac) {
    Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// ====================== FUNKCJE PAMIĘCI ======================
bool addPairedPeer() {
    if (!hasPairedRobot) {
        Serial.println("[PEER] addPairedPeer: no paired robot, skipping");
        return false;
    }
    Serial.print("[PEER] Removing old peer (if exists): ");
    printMac(pairedMac);
    Serial.println();
    esp_now_del_peer(pairedMac);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, pairedMac, 6);
    peerInfo.channel = pairedChannel;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;

    Serial.print("[PEER] Adding new peer: ");
    printMac(pairedMac);
    Serial.printf(", channel=%d\n", pairedChannel);

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[PEER] ERROR: Failed to add peer");
        hasPairedRobot = false;
        return false;
    }
    Serial.println("[PEER] Peer added successfully");
    return true;
}

void loadPairedMac() {
    prefs.begin("robot", true);
    if (prefs.getBytes("mac", pairedMac, 6) == 6) {
        hasPairedRobot = true;
        pairedChannel = prefs.getUChar("channel", pairing::DEFAULT_CHANNEL);
        Serial.print("[PEER] Loaded paired robot MAC: ");
        printMac(pairedMac);
        Serial.printf(", channel=%d\n", pairedChannel);
        WiFi.setChannel(pairedChannel, WIFI_SECOND_CHAN_NONE);
    } else {
        hasPairedRobot = false;
        pairedChannel = pairing::DEFAULT_CHANNEL;
        Serial.println("[PEER] No paired robot in flash, using default channel");
        WiFi.setChannel(pairing::DEFAULT_CHANNEL, WIFI_SECOND_CHAN_NONE);
    }
    prefs.end();
}

void savePairedMac(const uint8_t* mac, uint8_t channel) {
    prefs.begin("robot", false);
    prefs.putBytes("mac", mac, 6);
    prefs.putUChar("channel", channel);
    prefs.end();
    memcpy(pairedMac, mac, 6);
    pairedChannel = channel;
    hasPairedRobot = true;
    Serial.print("[PEER] Saved paired robot to flash: ");
    printMac(mac);
    Serial.printf(", channel=%d\n", channel);
}

// ====================== KOMENDY ======================
void sendCommand(Command cmd) {
    if (!hasPairedRobot) {
        Serial.println("[SEND] No paired robot, command ignored");
        return;
    }
    uint8_t data = static_cast<uint8_t>(cmd);
    Serial.printf("[SEND] Sending command 0x%02X to ", cmd);
    printMac(pairedMac);
    Serial.println();
    esp_now_send(pairedMac, &data, 1);
}

void sendCommandTo(const uint8_t* mac, Command cmd) {
    uint8_t data = static_cast<uint8_t>(cmd);
    Serial.printf("[SEND] Sending command 0x%02X to ", cmd);
    printMac(mac);
    Serial.println();
    esp_now_send(mac, &data, 1);
}

void sendPairRequest(const uint8_t* targetMac) {
    Serial.print("[PAIR] sendPairRequest to ");
    printMac(targetMac);
    Serial.printf(", switching to broadcast channel %d\n", pairing::BROADCAST_CHANNEL);
    WiFi.setChannel(pairing::BROADCAST_CHANNEL, WIFI_SECOND_CHAN_NONE);
    delay(20);

    esp_now_peer_info_t tempPeer = {};
    memcpy(tempPeer.peer_addr, targetMac, 6);
    tempPeer.channel = pairing::BROADCAST_CHANNEL;
    tempPeer.encrypt = false;
    tempPeer.ifidx = WIFI_IF_STA;

    Serial.print("[PEER] Removing existing peer (if any): ");
    printMac(targetMac);
    Serial.println();
    esp_now_del_peer(targetMac);

    Serial.print("[PEER] Adding temp peer for pair request: ");
    printMac(targetMac);
    Serial.printf(", channel=%d\n", pairing::BROADCAST_CHANNEL);
    if (esp_now_add_peer(&tempPeer) != ESP_OK) {
        Serial.println("[PEER] ERROR: Failed to add temp peer");
        return;
    }

    uint8_t data[1] = {static_cast<uint8_t>(Command::pairRequest)};
    Serial.println("[PAIR] Sending pairRequest command");
    esp_now_send(targetMac, data, 1);

    memcpy(pendingPairMac, targetMac, 6);
    waitingForPairResponse = true;
    pairingRequestTime = millis();
    Serial.println("[PAIR] Waiting for pairSuccess response (5s timeout)");
    Serial.printf("[PAIR] Current channel after send: %d\n", WiFi.channel());
}

void requestDataFromRobot() {
    if (currentScreen != Screen::Main) return;
    if (!hasPairedRobot) return;
    if (millis() - lastRequest > 100) {
        sendCommand(Command::requestDistanceData);
        lastRequest = millis();
    }
}

// ====================== OBSŁUGA BROADCAST ======================
void handlePairingBroadcast(const uint8_t* mac, const uint8_t* data, int len) {
    if (currentScreen != Screen::PairingMenu) return;
    char name[20] = "Unknown";
    if (len > 0) {
        int copyLen = std::min(len, (int)sizeof(name)-1);
        strncpy(name, (const char*)data, copyLen);
        name[copyLen] = '\0';
    }
    for (auto& robot : discoveredRobots) {
        if (memcmp(robot.mac, mac, 6) == 0) {
            strncpy(robot.name, name, sizeof(robot.name)-1);
            robot.lastSeen = millis();
            return;
        }
    }
    DiscoveredRobot r;
    memcpy(r.mac, mac, 6);
    strncpy(r.name, name, sizeof(r.name)-1);
    r.lastSeen = millis();
    discoveredRobots.push_back(r);
    Serial.print("[BROADCAST] New robot discovered: ");
    printMac(mac);
    Serial.printf(" name=%s\n", name);
}

void cleanupOldRobots() {
    if (currentScreen != Screen::PairingMenu) return;
    unsigned long now = millis();
    auto it = discoveredRobots.begin();
    while (it != discoveredRobots.end()) {
        if (now - it->lastSeen > 7000) {
            Serial.print("[BROADCAST] Removing stale robot: ");
            printMac(it->mac);
            Serial.println();
            it = discoveredRobots.erase(it);
        } else {
            ++it;
        }
    }
}

// ====================== TRANSFORMACJA DANYCH ======================
void transformDistanceData(uint16_t src[message::GRID_SIZE][message::GRID_SIZE]) {
    uint16_t temp[message::GRID_SIZE][message::GRID_SIZE];
    memcpy(temp, src, message::TOTAL_BYTES);
    
    for (uint8_t y = 0; y < message::GRID_SIZE; y++) {
        for (uint8_t x = 0; x < message::GRID_SIZE; x++) {
            distanceData[y][x] = temp[message::GRID_SIZE - 1 - x][y];
        }
    }
}

// ====================== ESP-NOW CALLBACK ======================
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
    Serial.print("[RX] from ");
    printMac(info->src_addr);
    Serial.printf(", len=%d, cmd=0x%02X\n", len, len>0 ? incomingData[0] : 0);

    if (len < 1) return;
    
    switch (static_cast<Command>(incomingData[0])) {
        case Command::distanceData:
            if (!hasPairedRobot || memcmp(info->src_addr, pairedMac, 6) != 0) {
                Serial.println("[RX] Ignoring distanceData from non-paired robot");
                return;
            }
            {
                uint16_t raw[message::GRID_SIZE][message::GRID_SIZE];
                memcpy(raw, incomingData + 1, message::TOTAL_BYTES);
                transformDistanceData(raw);
                if (currentScreen == Screen::Main) {
                    drawHeatmap(distanceData);
                }
                Serial.println("[RX] Distance data received and displayed");
            }
            break;

        case Command::broadcastPairing:
            handlePairingBroadcast(info->src_addr, incomingData + 1, len - 1);
            break;

        case Command::pairSuccess:
        {
            if (!waitingForPairResponse || memcmp(info->src_addr, pendingPairMac, 6) != 0) {
                Serial.print("[RX] Ignoring pairSuccess from unexpected MAC: ");
                printMac(info->src_addr);
                Serial.println();
                return;
            }

            Serial.println("[PAIR] pairSuccess received, finalizing pairing");
            
            esp_now_del_peer(pendingPairMac);

            waitingForPairResponse = false;
            uint8_t channel = pairing::DEFAULT_CHANNEL;
            if (len >= 2) channel = incomingData[1];

            savePairedMac(info->src_addr, channel);
            
            WiFi.setChannel(channel, WIFI_SECOND_CHAN_NONE);
            delay(80);

            if (addPairedPeer()) {
                currentScreen = Screen::Main;
                discoveredRobots.clear();
                selectedIndex = 0;
                drawHeatmap(distanceData);
                Serial.printf("[PAIR] Pairing SUCCESS → channel %d\n", channel);
            } else {
                Serial.println("[PAIR] ERROR: Failed to add paired peer after success");
            }
            break;
        }

        default:
            Serial.printf("[RX] Unknown command 0x%02X ignored\n", incomingData[0]);
            break;
    }
}

// ====================== OBSŁUGA PRZYCISKÓW ======================
void handlePairingInput() {
    static unsigned long lastInput = 0;
    if (millis() - lastInput < 180) return;
    bool needRedraw = false;

    if (digitalRead(pins::J_UP) == LOW) {
        if (selectedIndex > 0) {
            selectedIndex--;
            needRedraw = true;
        }
    }
    if (digitalRead(pins::J_DOWN) == LOW) {
        if (selectedIndex < (int)discoveredRobots.size() - 1) {
            selectedIndex++;
            needRedraw = true;
        }
    }
    if (digitalRead(pins::KEY_A) == LOW) {
        if (!discoveredRobots.empty()) {
            sendPairRequest(discoveredRobots[selectedIndex].mac);
            Serial.println("[PAIR] Pair request sent");
        }
    }
    if (digitalRead(pins::KEY_X) == LOW) {
        if (waitingForPairResponse) {
            Serial.println("[PAIR] Cancelling pair attempt (X pressed)");
            esp_now_del_peer(pendingPairMac);
            waitingForPairResponse = false;
            if (hasPairedRobot) {
                WiFi.setChannel(pairedChannel, WIFI_SECOND_CHAN_NONE);
            } else {
                WiFi.setChannel(pairing::DEFAULT_CHANNEL, WIFI_SECOND_CHAN_NONE);
            }
        }
        currentScreen = Screen::Main;
        discoveredRobots.clear();
        selectedIndex = 0;
        drawNotPaired();
        Serial.println("[MENU] Exited pairing menu");
    }
    if (needRedraw) drawPairingMenu(discoveredRobots, selectedIndex);
    if (digitalRead(pins::J_UP) == LOW || digitalRead(pins::J_DOWN) == LOW ||
        digitalRead(pins::KEY_A) == LOW || digitalRead(pins::KEY_B) == LOW) {
        lastInput = millis();
    }
}

void handleMovement() {
    static unsigned long lastSend = 0;
    const unsigned long interval = 120;
    if (!hasPairedRobot) return;
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

// ====================== SETUP ======================
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
    initDisplay();          // Inicjalizacja wyświetlacza (z display.cpp)
    setupKeys();
    setupJoystick();
    setupESPNow();
    loadPairedMac();
    addPairedPeer();
    drawNotPaired();        // Rysowanie ekranu „brak robota”
    Serial.println("[BOOT] Ready");
}

void loop() {
    cleanupOldRobots();

    if (waitingForPairResponse && (millis() - pairingRequestTime > 5000)) {
        Serial.println("[PAIR] Timeout - no pairSuccess received");
        esp_now_del_peer(pendingPairMac);
        waitingForPairResponse = false;
        if (hasPairedRobot) {
            WiFi.setChannel(pairedChannel, WIFI_SECOND_CHAN_NONE);
        } else {
            WiFi.setChannel(pairing::BROADCAST_CHANNEL, WIFI_SECOND_CHAN_NONE);
        }
        drawPairingMenu(discoveredRobots, selectedIndex);
    }

    static unsigned long lastMenuPress = 0;
    if (digitalRead(pins::KEY_Y) == LOW && digitalRead(pins::KEY_B) == LOW) {
        if (millis() - lastMenuPress > 400) {
            if (currentScreen == Screen::Main) {
                Serial.printf("[MENU] Enter pairing mode, switching to broadcast channel %d\n", pairing::BROADCAST_CHANNEL);
                WiFi.setChannel(pairing::BROADCAST_CHANNEL, WIFI_SECOND_CHAN_NONE);
                currentScreen = Screen::PairingMenu;
                discoveredRobots.clear();
                selectedIndex = 0;
                drawPairingMenu(discoveredRobots, selectedIndex);
            } else {
                if (waitingForPairResponse) {
                    esp_now_del_peer(pendingPairMac);
                    waitingForPairResponse = false;
                }
                if (hasPairedRobot) {
                    WiFi.setChannel(pairedChannel, WIFI_SECOND_CHAN_NONE);
                } else {
                    WiFi.setChannel(pairing::DEFAULT_CHANNEL, WIFI_SECOND_CHAN_NONE);
                }
                currentScreen = Screen::Main;
                discoveredRobots.clear();
                selectedIndex = 0;
                drawHeatmap(distanceData);
                Serial.println("[MENU] Exited pairing mode");
            }
            lastMenuPress = millis();
        }
    }

    if (currentScreen == Screen::PairingMenu) {
        handlePairingInput();
        if (currentScreen == Screen::PairingMenu) {
            static unsigned long lastDraw = 0;
            if (millis() - lastDraw > 250) {
                drawPairingMenu(discoveredRobots, selectedIndex);
                lastDraw = millis();
            }
        }
    } else {
        handleMovement();
        requestDataFromRobot();
    }
}
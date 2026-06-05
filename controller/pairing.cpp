#include "pairing.h"
#include "config.h"
#include "commands.h"
#include <esp_now.h>
#include <WiFi.h>
#include <Preferences.h>
#include <cstring>
#include <algorithm>

static std::vector<DiscoveredRobot> discoveredRobots;
static int selectedIndex{};
static Screen currentScreen{ Screen::Main };

static bool hasPairedRobot{ false };
static uint8_t pairedMac[6];
static uint8_t pairedChannel{ pairing::DEFAULT_CHANNEL };

static bool waitingForPairResponse{ false };
static uint8_t pendingPairMac[6];
static unsigned long pairingRequestTime{};

static Preferences prefs;

void printMac(const uint8_t* mac) {
    Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static bool addPairedPeer() {
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

static void loadPairedMac() {
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

static void savePairedMac(const uint8_t* mac, uint8_t channel) {
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

static void sendPairRequest(const uint8_t* targetMac) {
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
}

static void handlePairingBroadcast(const uint8_t* mac, const uint8_t* data, int len) {
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

static void cleanupOldRobots() {
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

void handlePairingInput() {
    static unsigned long lastInput = 0;
    static unsigned long lastDraw = 0;
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
        exitPairingMode();
    }

    if (needRedraw) {
        drawPairingMenu(discoveredRobots, selectedIndex);
    }
    if (millis() - lastDraw > 250) {
        drawPairingMenu(discoveredRobots, selectedIndex);
        lastDraw = millis();
    }

    if (digitalRead(pins::J_UP) == LOW || digitalRead(pins::J_DOWN) == LOW ||
        digitalRead(pins::KEY_A) == LOW || digitalRead(pins::KEY_B) == LOW) {
        lastInput = millis();
    }
}

void initPairing() {
    loadPairedMac();
    addPairedPeer();
}

void updatePairing() {
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
}

void handlePairingReceive(const uint8_t* srcMac, const uint8_t* data, int len) {
    if (len < 1) return;
    Command cmd = static_cast<Command>(data[0]);

    switch (cmd) {
        case Command::broadcastPairing:
            handlePairingBroadcast(srcMac, data + 1, len - 1);
            break;

        case Command::pairSuccess:
        {
          if (!waitingForPairResponse || memcmp(srcMac, pendingPairMac, 6) != 0) {
              Serial.print("[RX] Ignoring pairSuccess from unexpected MAC: ");
              printMac(srcMac);
              Serial.println();
              return;
          }
          Serial.println("[PAIR] pairSuccess received, finalizing pairing");
          esp_now_del_peer(pendingPairMac);
          waitingForPairResponse = false;
          uint8_t channel = pairing::DEFAULT_CHANNEL;
          if (len >= 2) channel = data[1];
          savePairedMac(srcMac, channel);
          WiFi.setChannel(channel, WIFI_SECOND_CHAN_NONE);
          delay(80);
          if (addPairedPeer()) {
              currentScreen = Screen::Main;
              discoveredRobots.clear();
              selectedIndex = 0;
              drawNotPaired();
              Serial.printf("[PAIR] Pairing SUCCESS → channel %d\n", channel);
          } else {
              Serial.println("[PAIR] ERROR: Failed to add paired peer after success");
          }
          break;
        }
    }
}

void enterPairingMode() {
    Serial.printf("[MENU] Enter pairing mode, switching to broadcast channel %d\n", pairing::BROADCAST_CHANNEL);
    WiFi.setChannel(pairing::BROADCAST_CHANNEL, WIFI_SECOND_CHAN_NONE);
    currentScreen = Screen::PairingMenu;
    discoveredRobots.clear();
    selectedIndex = 0;
    drawPairingMenu(discoveredRobots, selectedIndex);
}

void exitPairingMode() {
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
    drawNotPaired();
    Serial.println("[MENU] Exited pairing mode");
}

bool isPairingMode() {
    return currentScreen == Screen::PairingMenu;
}

bool isPaired() {
    return hasPairedRobot;
}

const uint8_t* getPairedMac() {
    return hasPairedRobot ? pairedMac : nullptr;
}

uint8_t getPairedChannel() {
    return pairedChannel;
}

void sendCommandToPaired(Command cmd) {
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
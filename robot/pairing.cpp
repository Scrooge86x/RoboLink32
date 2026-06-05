#include "pairing.h"

#include "config.h"
#include "commands.h"

#include <cstring>
#include <esp_now.h>
#include <WiFi.h>
#include <Preferences.h>

namespace {

const uint8_t broadcastMac[]{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
Preferences prefs{};

bool isPairingActive{};
unsigned long pairingStartTime{};
unsigned long lastAnnounce{};
bool skipAnnounce{false};

bool hasPaired{};
uint8_t pairedMac[6]{};
uint8_t pairedChannel{};

void printMac(const uint8_t* mac) {
    Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void updatePeer() {
    if (hasPaired) {
        Serial.print("[PEER] Removing old peer: ");
        printMac(pairedMac);
        Serial.println();
        esp_now_del_peer(pairedMac);
    }

    esp_now_peer_info_t peerInfo{};
    memcpy(peerInfo.peer_addr, pairedMac, 6);
    peerInfo.channel = pairedChannel;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;

    Serial.print("[PEER] Adding new peer: ");
    printMac(pairedMac);
    Serial.printf(", channel=%d\n", pairedChannel);

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[PEER] ERROR: Failed to add peer");
        hasPaired = false;
        return;
    }

    WiFi.setChannel(pairedChannel);
    hasPaired = true;
    Serial.println("[PEER] Peer added successfully, channel set");
}

void savePeerToFlash() {
    prefs.begin("robot", false);
    prefs.putBytes("mac", pairedMac, 6);
    prefs.putUChar("channel", pairedChannel);
    prefs.end();
    Serial.print("[FLASH] Saved peer MAC: ");
    printMac(pairedMac);
    Serial.printf(", channel=%d\n", pairedChannel);
}

void loadPeerFromFlash() {
    prefs.begin("robot", true);
    if (prefs.getBytes("mac", pairedMac, 6) == 6) {
        pairedChannel = prefs.getUChar("channel", 0);
        Serial.print("[FLASH] Loaded peer MAC: ");
        printMac(pairedMac);
        Serial.printf(", channel=%d\n", pairedChannel);
        updatePeer();
    } else {
        hasPaired = false;
        Serial.println("[FLASH] No paired robot found in flash");
    }
    prefs.end();
}

void addBroadcastPeer() {
    esp_now_peer_info_t broadcastPeer = {};
    memcpy(broadcastPeer.peer_addr, broadcastMac, 6);
    broadcastPeer.channel = 0;
    broadcastPeer.encrypt = false;
    broadcastPeer.ifidx = WIFI_IF_STA;
    if (esp_now_add_peer(&broadcastPeer) != ESP_OK) {
        Serial.println("[PEER] Failed to add broadcast peer");
    } else {
        Serial.println("[PEER] Broadcast peer (FF:FF:FF:FF:FF:FF) added");
    }
}

} // anonymous

namespace pairing {

void setup() {
    Serial.println("[PAIRING] Setup started");
    loadPeerFromFlash();
    addBroadcastPeer();
    Serial.println("[PAIRING] Setup complete");
}

void start() {
    if (isPairingActive) {
        Serial.println("[PAIRING] Already active, ignoring start");
        return;
    }

    isPairingActive = true;
    pairingStartTime = millis();
    lastAnnounce = 0;
    skipAnnounce = false;
    Serial.printf("[PAIRING] Starting pairing mode, channel set to %d (broadcast channel)\n", pairing::broadcastChannel);
    WiFi.setChannel(pairing::broadcastChannel);
    Serial.printf("[PAIRING] Actual channel after set: %d\n", WiFi.channel());
}

void update() {
    if (!isPairingActive) return;

    unsigned long now{ millis() };
    if (now - pairingStartTime >= pairing::timeoutMs) {
        Serial.println("[PAIRING] Pairing timeout, deactivating");
        isPairingActive = false;
        skipAnnounce = false;
        WiFi.setChannel(hasPaired ? pairedChannel : 1);
        return;
    }

    if (!skipAnnounce && (now - lastAnnounce >= pairing::announceIntervalMs)) {
        lastAnnounce = now;

        constexpr size_t bufferSize{ 1 + pairing::maxNameLength };
        uint8_t data[bufferSize]{ static_cast<uint8_t>(Command::broadcastPairing) };
        strlcpy(reinterpret_cast<char*>(data + 1), pairing::robotName, bufferSize - 1);
        size_t nameLen = strnlen(pairing::robotName, pairing::maxNameLength);
        Serial.printf("[BROADCAST] Sending announcement (name=%s, len=%d)\n", pairing::robotName, nameLen);
        esp_now_send(broadcastMac, data, 1 + nameLen);
    }
}

void onDataRecv(const uint8_t* srcMac, const uint8_t* data, int len) {
    Serial.print("[RX] from ");
    printMac(srcMac);
    Serial.printf(", len=%d, cmd=0x%02X, pairingActive=%d\n", len, data[0], isPairingActive);

    if (!isPairingActive || len < 1) return;

    if (static_cast<Command>(data[0]) != Command::pairRequest) {
        Serial.println("[RX] Not a pairRequest, ignoring");
        return;
    }

    Serial.println("[PAIR] Received pairRequest! Starting pairing handshake");

    skipAnnounce = true;

    Serial.print("[PEER] Removing existing peer (if any): ");
    printMac(srcMac);
    Serial.println();
    esp_now_del_peer(srcMac);
    delay(20);

    int newChannel{2};
    // do {
    //     newChannel = random(1, 12);
    // } while (newChannel == pairing::broadcastChannel);
    Serial.printf("[PAIR] Selected new channel for robot: %d\n", newChannel);

    // Temp peer
    esp_now_peer_info_t tempPeer = {};
    memcpy(tempPeer.peer_addr, srcMac, 6);
    tempPeer.channel = WiFi.channel();
    tempPeer.encrypt = false;
    tempPeer.ifidx = WIFI_IF_STA;
    Serial.print("[PEER] Adding temp peer for pairSuccess: ");
    printMac(srcMac);
    Serial.printf(", channel=%d\n", WiFi.channel());
    if (esp_now_add_peer(&tempPeer) != ESP_OK) {
        Serial.println("[PEER] ERROR: Failed to add temp peer");
        skipAnnounce = false;
        return;
    }
    delay(30);

    // Send pairSuccess
    uint8_t pairSuccessResponse[2] = {
        static_cast<uint8_t>(Command::pairSuccess),
        static_cast<uint8_t>(newChannel)
    };

    Serial.print("[PAIR] Sending pairSuccess to ");
    printMac(srcMac);
    Serial.printf(", newChannel=%d, current channel=%d\n", newChannel, WiFi.channel());

    esp_err_t res = esp_now_send(srcMac, pairSuccessResponse, 2);
    Serial.printf("[PAIR] esp_now_send result: %d (%s)\n", res, res == ESP_OK ? "OK" : "FAIL");

    delay(1000);

    if (res == ESP_OK) {
        delay(40);
        esp_now_send(srcMac, pairSuccessResponse, 2);
        Serial.println("[PAIR] Sent duplicate pairSuccess");
    }

    delay(30);

    // Cleanup temp peer
    Serial.print("[PEER] Removing temp peer: ");
    printMac(srcMac);
    Serial.println();
    esp_now_del_peer(srcMac);
    delay(20);

    // Save and switch channel
    memcpy(pairedMac, srcMac, 6);
    pairedChannel = newChannel;
    savePeerToFlash();

    Serial.printf("[WIFI] Switching robot channel from %d to %d\n", WiFi.channel(), newChannel);
    WiFi.setChannel(newChannel);
    delay(60);  // stabilizacja kanału

    // Add permanent peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, pairedMac, 6);
    peerInfo.channel = pairedChannel;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;
    Serial.print("[PEER] Adding permanent peer: ");
    printMac(pairedMac);
    Serial.printf(", channel=%d\n", pairedChannel);
    if (esp_now_add_peer(&peerInfo) == ESP_OK) {
        hasPaired = true;
        Serial.println("[PEER] Permanent peer added successfully");
    } else {
        Serial.println("[PEER] ERROR: Failed to add permanent peer");
        hasPaired = false;
    }

    isPairingActive = false;
    skipAnnounce = false;
    Serial.println("[PAIR] Pairing mode deactivated, ready for normal operation");
}

bool isActive() {
    return isPairingActive;
}

bool isPaired() {
    return hasPaired;
}

const uint8_t* getPairedMac() {
    return hasPaired ? pairedMac : nullptr;
}

} // pairing
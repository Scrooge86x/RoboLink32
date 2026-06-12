#include "pairing.h"

#include "config.h"
#include "commands.h"

#include <cstring>
#include <esp_now.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_mac.h>

namespace {

const uint8_t broadcastMac[]{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
Preferences prefs{};

bool isPairingActive{};
unsigned long pairingStartTime{};
unsigned long lastAnnounce{};
bool skipAnnounce{ false };

bool hasPaired{};
uint8_t pairedMac[6]{};
uint8_t pairedChannel{};

bool addPeer(const uint8_t* mac, uint8_t channel, const char* description = "peer") {
    esp_now_peer_info_t peerInfo{};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = channel;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;

    Serial.printf("[PEER] Adding %s: " MACSTR ", channel=%d\n",
                  description, MAC2STR(mac), channel);

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.printf("[PEER] ERROR: Failed to add %s\n", description);
        return false;
    }

    Serial.printf("[PEER] %s added successfully\n", description);
    return true;
}

bool setPairedPeer(const uint8_t* mac, uint8_t channel) {
    if (hasPaired) {
        Serial.printf("[PEER] Removing old paired peer: " MACSTR "\n", MAC2STR(pairedMac));
        esp_now_del_peer(pairedMac);
        hasPaired = false;
    }

    if (!addPeer(mac, channel, "paired robot")) {
        hasPaired = false;
        return false;
    }

    memcpy(pairedMac, mac, 6);
    pairedChannel = channel;
    hasPaired = true;

    prefs.begin("robot", false);
    prefs.putBytes("mac", pairedMac, 6);
    prefs.putUChar("channel", pairedChannel);
    prefs.end();
    Serial.printf("[FLASH] Saved peer MAC: " MACSTR ", channel=%d\n",
                    MAC2STR(pairedMac), pairedChannel);

    WiFi.setChannel(channel);
    Serial.printf("[PEER] Channel set to %d (paired)\n", channel);

    return true;
}

void loadPeerFromFlash() {
    prefs.begin("robot", true);
    if (prefs.getBytes("mac", pairedMac, 6) == 6) {
        pairedChannel = prefs.getUChar("channel", 0);
        prefs.end();
        Serial.printf("[FLASH] Loaded peer MAC: " MACSTR ", channel=%d\n",
                      MAC2STR(pairedMac), pairedChannel);
        setPairedPeer(pairedMac, pairedChannel);
    } else {
        hasPaired = false;
        prefs.end();
        Serial.println("[FLASH] No paired robot found in flash");
    }
}

} // anonymous

namespace pairing {

void setup() {
    Serial.println("[PAIRING] Setup started");

    loadPeerFromFlash();
    addPeer(broadcastMac, broadcastChannel, "broadcast peer");

    Serial.println("[PAIRING] Setup complete");
}

void start() {
    if (isPairingActive) {
        Serial.println("[PAIRING] Already active, ignoring start");
        return;
    }

    if (hasPaired) {
        Serial.printf("[PAIRING] Unpairing current device " MACSTR "\n", MAC2STR(pairedMac));
        esp_now_del_peer(pairedMac);
        hasPaired = false;
        memset(pairedMac, 0, sizeof(pairedMac));
        pairedChannel = 0;
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
    if (!isPairingActive) {
        return;
    }

    unsigned long now{ millis() };
    if (now - pairingStartTime >= pairing::timeoutMs) {
        Serial.println("[PAIRING] Pairing timeout, deactivating");
        isPairingActive = false;
        skipAnnounce = false;
        WiFi.setChannel(hasPaired ? pairedChannel : broadcastChannel);
        return;
    }

    if (!skipAnnounce && (now - lastAnnounce >= pairing::announceIntervalMs)) {
        lastAnnounce = now;

        constexpr size_t bufferSize{ 1 + sizeof(pairing::robotName) };
        uint8_t data[bufferSize]{ static_cast<uint8_t>(Command::broadcastPairing) };
        strlcpy(reinterpret_cast<char*>(data + 1), pairing::robotName, bufferSize - 1);
        Serial.printf("[BROADCAST] Sending announcement\n");
        esp_now_send(broadcastMac, data, bufferSize);
    }
}

void onDataRecv(const uint8_t* srcMac, const uint8_t* data, int len) {
    Serial.printf("[RX] from " MACSTR ", len=%d, cmd=0x%02X, pairingActive=%d\n",
                  MAC2STR(srcMac), len, data[0], isPairingActive);

    if (!isPairingActive || len < 1) {
        return;
    }

    if (static_cast<Command>(data[0]) != Command::pairRequest) {
        Serial.println("[RX] Not a pairRequest, ignoring");
        return;
    }

    Serial.println("[PAIR] Received pairRequest! Starting pairing handshake");

    skipAnnounce = true;

    Serial.printf("[PEER] Removing any previous temp peer: " MACSTR "\n", MAC2STR(srcMac));
    esp_now_del_peer(srcMac);

    // Channels higher than 2 don't seem to work
    uint8_t newChannel{ 2 };
    // do {
    //     newChannel = random(1, 12);
    // } while (newChannel == pairing::broadcastChannel);
    Serial.printf("[PAIR] Selected new channel for robot: %d\n", newChannel);

    if (!addPeer(srcMac, WiFi.channel(), "temp peer for pairSuccess")) {
        skipAnnounce = false;
        return;
    }

    uint8_t pairSuccessResponse[2]{ static_cast<uint8_t>(Command::pairSuccess), newChannel };

    Serial.printf("[PAIR] Sending pairSuccess to " MACSTR ", newChannel=%d, current channel=%d\n",
                  MAC2STR(srcMac), newChannel, WiFi.channel());

    esp_err_t res{ esp_now_send(srcMac, pairSuccessResponse, 2) };
    Serial.printf("[PAIR] esp_now_send result: %d (%s)\n", res, res == ESP_OK ? "OK" : "FAIL");

    if (res == ESP_OK) {
        delay(40);
        esp_now_send(srcMac, pairSuccessResponse, 2);
        Serial.println("[PAIR] Sent duplicate pairSuccess");
    }

    Serial.printf("[PEER] Removing temp peer: " MACSTR "\n", MAC2STR(srcMac));
    esp_now_del_peer(srcMac);
    delay(20);

    Serial.printf("[WIFI] Switching robot channel from %d to %d\n", WiFi.channel(), newChannel);
    WiFi.setChannel(newChannel);

    if (setPairedPeer(srcMac, newChannel)) {
        Serial.println("[PEER] Permanent peer added successfully");
    } else {
        Serial.println("[PEER] ERROR: Failed to add permanent peer");
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

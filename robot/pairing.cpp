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

bool hasPaired{};
uint8_t pairedMac[6]{};
uint8_t pairedChannel{};

void updatePeer() {
    if (hasPaired && esp_now_del_peer(pairedMac) != ESP_OK) {
        Serial.println("Failed to delete old peer");
    }

    esp_now_peer_info_t peerInfo{};
    memcpy(peerInfo.peer_addr, pairedMac, 6);
    peerInfo.channel = pairedChannel;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add new peer");
        hasPaired = false;
        return;
    }

    WiFi.setChannel(pairedChannel);
    hasPaired = true;
}

void savePeerToFlash() {
    prefs.begin("robot", false);
    prefs.putBytes("mac", pairedMac, 6);
    prefs.putUChar("channel", pairedChannel);
    prefs.end();
}

void loadPeerFromFlash() {
    prefs.begin("robot", true);
    if (prefs.getBytes("mac", pairedMac, 6) == 6) {
        pairedChannel = prefs.getUChar("channel", 0);
        updatePeer();
    } else {
        hasPaired = false;
    }
    prefs.end();
}

} // anonymous

namespace pairing {

void setup() {
    loadPeerFromFlash();
}

void start() {
    if (isPairingActive) {
        return;
    }

    isPairingActive = true;
    pairingStartTime = millis();
    lastAnnounce = 0;
    WiFi.setChannel(pairing::broadcastChannel);
}

void update() {
    if (!isPairingActive) {
        return;
    }

    unsigned long now{ millis() };
    if (now - pairingStartTime >= pairing::timeoutMs) {
        isPairingActive = false;
        WiFi.setChannel(hasPaired ? pairedChannel : 1);
        return;
    }

    if (now - lastAnnounce >= pairing::announceIntervalMs) {
        lastAnnounce = now;

        constexpr size_t bufferSize{ 1 + pairing::maxNameLength };
        uint8_t data[bufferSize]{ static_cast<uint8_t>(Command::broadcastPairing) };
        strncpy(reinterpret_cast<char*>(data + 1), pairing::robotName, bufferSize - 2);

        constexpr size_t robotNameLength{ sizeof(pairing::robotName) - 1 };
        esp_now_send(broadcastMac, data, 1 + robotNameLength);
    }
}

void onDataRecv(const uint8_t* srcMac, const uint8_t* data, int len) {
    if (!isPairingActive || len < 1) {
        return;
    }

    if (static_cast<Command>(data[0]) != Command::pairRequest) {
        return;
    }

    int newChannel{};
    do {
        newChannel = random(1, 12);
    } while (newChannel == pairing::broadcastChannel);

    memcpy(pairedMac, srcMac, 6);
    pairedChannel = newChannel;

    savePeerToFlash();
    updatePeer();
    if (!hasPaired) {
        return;
    }

    uint8_t pairSuccessResponse[2]{ static_cast<uint8_t>(Command::pairSuccess), static_cast<uint8_t>(newChannel) };
    esp_now_send(srcMac, pairSuccessResponse, 2);

    isPairingActive = false;
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
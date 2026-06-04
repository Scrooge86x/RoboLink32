#include "config.h"
#include "commands.h"
#include <TFT_eSPI.h>
#include <SPI.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Preferences.h>
#include <vector>
#include <algorithm>
#include <cstring>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

uint16_t distanceData[message::GRID_SIZE][message::GRID_SIZE]{};

unsigned long lastRequest = 0;

// ====================== PAROWANIE ======================
struct DiscoveredRobot {
    uint8_t mac[6];
    char name[20];
    unsigned long lastSeen;
};

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

// ====================== FUNKCJE PAMIĘCI ======================
void addPairedPeer() {
    if (!hasPairedRobot) return;
    esp_now_del_peer(pairedMac);
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, pairedMac, 6);
    peerInfo.channel = pairedChannel;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        hasPairedRobot = false;
    } else {
        Serial.println("Paired robot added as peer");
    }
}

void loadPairedMac() {
    prefs.begin("robot", true);
    if (prefs.getBytes("mac", pairedMac, 6) == 6) {
        hasPairedRobot = true;
        pairedChannel = prefs.getUChar("channel", pairing::DEFAULT_CHANNEL);
        WiFi.setChannel(pairedChannel, WIFI_SECOND_CHAN_NONE);
        Serial.printf("Loaded paired robot, channel %d\n", pairedChannel);
    } else {
        hasPairedRobot = false;
        pairedChannel = pairing::DEFAULT_CHANNEL;
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
    Serial.printf("Saved paired robot, channel %d\n", channel);
}

// ====================== KOMENDY ======================
void sendCommand(Command cmd) {
    if (!hasPairedRobot) return;
    uint8_t data = static_cast<uint8_t>(cmd);
    esp_now_send(pairedMac, &data, 1);
}

void sendCommandTo(const uint8_t* mac, Command cmd) {
    uint8_t data = static_cast<uint8_t>(cmd);
    esp_now_send(mac, &data, 1);
}

void sendPairRequest(const uint8_t* targetMac) {
    WiFi.setChannel(pairing::BROADCAST_CHANNEL, WIFI_SECOND_CHAN_NONE);

    esp_now_peer_info_t tempPeer = {};
    memcpy(tempPeer.peer_addr, targetMac, 6);
    tempPeer.channel = pairing::BROADCAST_CHANNEL;
    tempPeer.encrypt = false;
    tempPeer.ifidx = WIFI_IF_STA;
    if (esp_now_add_peer(&tempPeer) != ESP_OK) {
        Serial.println("Failed to add temp peer for pair request");
        return;
    }

    uint8_t data[1] = {static_cast<uint8_t>(Command::pairRequest)};
    esp_now_send(targetMac, data, 1);
    Serial.println("Pair request sent on broadcast channel");

    esp_now_del_peer(targetMac);
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
            robot.name[sizeof(robot.name)-1] = '\0';
            robot.lastSeen = millis();
            return;
        }
    }
    DiscoveredRobot r;
    memcpy(r.mac, mac, 6);
    strncpy(r.name, name, sizeof(r.name)-1);
    r.name[sizeof(r.name)-1] = '\0';
    r.lastSeen = millis();
    discoveredRobots.push_back(r);
}

void cleanupOldRobots() {
    if (currentScreen != Screen::PairingMenu) return;
    unsigned long now = millis();
    discoveredRobots.erase(
        std::remove_if(discoveredRobots.begin(), discoveredRobots.end(),
            [now](const DiscoveredRobot& r){ return now - r.lastSeen > 7000; }),
        discoveredRobots.end()
    );
}

// ====================== TRANSFORMACJA DANYCH ======================
void transformDistanceData(uint16_t src[message::GRID_SIZE][message::GRID_SIZE]) {
    uint16_t temp[message::GRID_SIZE][message::GRID_SIZE];
    memcpy(temp, src, message::TOTAL_BYTES);
    for (uint8_t y = 0; y < message::GRID_SIZE; y++) {
        for (uint8_t x = 0; x < message::GRID_SIZE; x++) {
            distanceData[y][x] = temp[x][message::GRID_SIZE - 1 - y];
        }
    }
}

// ====================== ESP-NOW CALLBACK ======================
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
    Serial.printf("RX from %02X:%02X:%02X:%02X:%02X:%02X, len=%d, data[0]=0x%02X\n",
        info->src_addr[0], info->src_addr[1], info->src_addr[2],
        info->src_addr[3], info->src_addr[4], info->src_addr[5],
        len,
        len > 0 ? incomingData[0] : 0
    );

    if (len < 1) return;
    
    switch (static_cast<Command>(incomingData[0])) {
        case Command::distanceData:
            {
                uint16_t raw[message::GRID_SIZE][message::GRID_SIZE];
                memcpy(raw, incomingData + 1, message::TOTAL_BYTES);
                transformDistanceData(raw);
                if (currentScreen == Screen::Main) {
                    drawMainScreen();
                }
            }
            break;
        case Command::broadcastPairing:
            handlePairingBroadcast(info->src_addr, incomingData + 1, len - 1);
            break;
        case Command::pairSuccess:
            {
                uint8_t channel = pairing::DEFAULT_CHANNEL;
                if (len >= 2) channel = incomingData[1];
                savePairedMac(info->src_addr, channel);
                WiFi.setChannel(channel, WIFI_SECOND_CHAN_NONE);
                addPairedPeer();
                currentScreen = Screen::Main;
                discoveredRobots.clear();
                selectedIndex = 0;
                drawMainScreen();
                Serial.printf("Pairing SUCCESS, channel %d\n", channel);
            }
            break;
        default:
            break;
    }
}

// ====================== RYSOWANIE ======================
uint16_t hslToColor565(float h, float s, float l) {
    float r, g, b;
    if (s == 0) {
        r = g = b = l;
    } else {
        auto hue2rgb = [](float p, float q, float t) -> float {
            if (t < 0) t += 1;
            if (t > 1) t -= 1;
            if (t < 1.0f/6) return p + (q - p) * 6 * t;
            if (t < 1.0f/2) return q;
            if (t < 2.0f/3) return p + (q - p) * (2.0f/3 - t) * 6;
            return p;
        };
        float q = l < 0.5f ? l * (1 + s) : l + s - l * s;
        float p = 2 * l - q;
        r = hue2rgb(p, q, h + 1.0f/3);
        g = hue2rgb(p, q, h);
        b = hue2rgb(p, q, h - 1.0f/3);
    }
    uint8_t red   = (uint8_t)(r * 255);
    uint8_t green = (uint8_t)(g * 255);
    uint8_t blue  = (uint8_t)(b * 255);
    return spr.color565(red, green, blue);
}

uint16_t getHeatmapColor(uint16_t dist) {
    if (dist < 0) return TFT_BLACK;
    int16_t clamped = constrain(dist, heatmap::MIN_DISTANCE, heatmap::MAX_DISTANCE);
    uint8_t hue = map(clamped, heatmap::MIN_DISTANCE, heatmap::MAX_DISTANCE, 0, 240);
    return hslToColor565(hue / 360.0f, 1.0f, 0.5f);
}

void drawNotPaired() {
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(20, 120);
    tft.print("No robot paired");    
}

void drawMainScreen() {
    spr.fillSprite(heatmap::COLOR_BG);
    constexpr uint8_t grid = message::GRID_SIZE;
    constexpr uint16_t cellWidth  = lcdscreen::WIDTH  / grid;
    constexpr uint16_t cellHeight = lcdscreen::HEIGHT / grid;

    for (uint8_t y = 0; y < grid; y++) {
        for (uint8_t x = 0; x < grid; x++) {
            uint16_t dist = distanceData[y][x];
            uint16_t color = getHeatmapColor(dist);
            spr.fillRect(x * cellWidth, y * cellHeight, cellWidth, cellHeight, color);
        }
    }
    spr.pushSprite(0, 0);
}

void drawPairingMenu() {
    spr.fillSprite(TFT_BLACK);
    spr.setTextColor(TFT_WHITE);
    spr.setTextSize(2);
    spr.drawString("Select Robot:", 20, 10);

    if (discoveredRobots.empty()) {
        spr.setTextColor(TFT_RED);
        spr.drawString("No robots found", 20, 80);
        spr.pushSprite(0, 0);
        return;
    }

    for (size_t i = 0; i < discoveredRobots.size() && i < 7; ++i) {
        const auto& robot = discoveredRobots[i];
        bool selected = (i == selectedIndex);
        uint16_t color = selected ? TFT_YELLOW : TFT_WHITE;
        uint16_t bg    = selected ? TFT_NAVY   : TFT_BLACK;
        char line[32];
        sprintf(line, "%s", robot.name);
        spr.setTextColor(color, bg);
        spr.drawString(line, 20, 50 + i * 28);
    }
    spr.pushSprite(0, 0);
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
            Serial.println("→ Pair request sent");
        }
    }
    if (digitalRead(pins::KEY_X) == LOW) {
        if (hasPairedRobot) {
            WiFi.setChannel(pairedChannel, WIFI_SECOND_CHAN_NONE);
        } else {
            WiFi.setChannel(pairing::DEFAULT_CHANNEL, WIFI_SECOND_CHAN_NONE);
        }
        currentScreen = Screen::Main;
        discoveredRobots.clear();
        selectedIndex = 0;
        drawNotPaired();
        Serial.println("Exited pairing menu");
    }
    if (needRedraw) drawPairingMenu();
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
void setupScreen() {
    pinMode(pins::TFT_BL_PIN, OUTPUT);
    digitalWrite(pins::TFT_BL_PIN, HIGH);
    tft.init();
    tft.setRotation(lcdscreen::ROTATION);
    if (!spr.createSprite(lcdscreen::WIDTH, lcdscreen::HEIGHT)) {
        Serial.println("Sprite initialization failed!");
    }
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
    WiFi.mode(espnow::WIFI_MODE);
    WiFi.setChannel(pairing::DEFAULT_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    esp_now_register_recv_cb(OnDataRecv);

    esp_now_peer_info_t broadcastPeer = {};
    memset(broadcastPeer.peer_addr, 0xFF, 6);
    broadcastPeer.channel = 0;
    broadcastPeer.encrypt = false;
    broadcastPeer.ifidx = WIFI_IF_STA;
    if (esp_now_add_peer(&broadcastPeer) != ESP_OK) {
        Serial.println("Failed to add broadcast peer");
    } else {
        Serial.println("Broadcast peer added");
    }
}

void setup() {
    Serial.begin(115200);
    setupScreen();
    setupKeys();
    setupJoystick();
    setupESPNow();
    loadPairedMac();
    addPairedPeer();
    drawNotPaired();
    Serial.println("Initialization completed.");
}

void loop() {
    cleanupOldRobots();
    static unsigned long lastMenuPress = 0;
    if (digitalRead(pins::KEY_Y) == LOW && digitalRead(pins::KEY_B) == LOW) {
        if (millis() - lastMenuPress > 400) {
            if (currentScreen == Screen::Main) {
                Serial.printf("Channel before set: %d\n", WiFi.channel());
                WiFi.setChannel(pairing::BROADCAST_CHANNEL, WIFI_SECOND_CHAN_NONE);
                Serial.printf("Channel after set: %d\n", WiFi.channel());
                currentScreen = Screen::PairingMenu;
                discoveredRobots.clear();
                selectedIndex = 0;
                Serial.println("Entered pairing menu");
                drawPairingMenu();
            } else {
                if (hasPairedRobot) {
                    WiFi.setChannel(pairedChannel, WIFI_SECOND_CHAN_NONE);
                } else {
                    WiFi.setChannel(pairing::DEFAULT_CHANNEL, WIFI_SECOND_CHAN_NONE);
                }
                currentScreen = Screen::Main;
                discoveredRobots.clear();
                selectedIndex = 0;
                drawMainScreen();
                Serial.println("Exited pairing menu");
            }
            lastMenuPress = millis();
        }
    }
    if (currentScreen == Screen::PairingMenu) {
        handlePairingInput();
        if (currentScreen == Screen::PairingMenu) {
            static unsigned long lastDraw = 0;
            if (millis() - lastDraw > 250) {
                drawPairingMenu();
                lastDraw = millis();
            }
        }
    } else {
        handleMovement();
        requestDataFromRobot();
    }
}
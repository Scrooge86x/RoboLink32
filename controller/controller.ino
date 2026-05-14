#include "config.h"
#include "commands.h"
#include <TFT_eSPI.h>
#include <SPI.h>
#include <esp_now.h>
#include <WiFi.h>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

uint16_t distanceData[message::GRID_SIZE][message::GRID_SIZE]{};

constexpr int16_t MIN_DISTANCE{20};
constexpr int16_t MAX_DISTANCE{200};
const uint16_t COLOR_BG = TFT_BLACK;

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

// =======================================================

void addPairedPeer() {
    if (!espnow::hasPairedRobot) return;
    
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, espnow::pairedMac, 6);
    peerInfo.channel = 0;           // 0 = current channel
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
    } else {
        Serial.println("Paired robot added as peer");
    }
}

void loadPairedMac() {
    prefs.begin("robot", true);
    if (prefs.getBytes("mac", espnow::pairedMac, 6) == 6) {
        espnow::hasPairedRobot = true;
        uint8_t savedChannel = prefs.getUChar("channel", espnow::WIFI_CHANNEL);
        WiFi.setChannel(savedChannel, WIFI_SECOND_CHAN_NONE);
        Serial.println("Loaded paired robot MAC");
    }
    prefs.end();
}

void savePairedMac(const uint8_t* mac) {
    prefs.begin("robot", false);
    prefs.putBytes("mac", mac, 6);
    prefs.end();
    memcpy(espnow::pairedMac, mac, 6);
    espnow::hasPairedRobot = true;
    Serial.println("Paired robot saved to flash");
}

// =======================================================

void printDistanceGrid() {

  Serial.println(F("=== Distance Grid ==="));

  for (uint8_t y = 0; y < message::GRID_SIZE; y++) {
        for (uint8_t x = 0; x < message::GRID_SIZE; x++) {
            int16_t dist = distanceData[y][x];

            Serial.print(dist);
            Serial.print(F("  "));
        }
        Serial.println();
    }
  
  Serial.println(F("====================="));
}

void sendCommand(Command cmd) {
  if (!espnow::hasPairedRobot) {
    return;
  }

  uint8_t data = static_cast<uint8_t>(cmd);
  esp_now_send(espnow::pairedMac, &data, 1);
}

void sendCommandTo(const uint8_t* mac, Command cmd) {
    uint8_t data = static_cast<uint8_t>(cmd);
    esp_now_send(mac, &data, 1);
}

void sendPairRequest(const uint8_t* targetMac) {
    uint8_t data[1] = {static_cast<uint8_t>(Command::pairRequest)};
    esp_now_send(targetMac, data, 1);
}

void requestDataFromRobot() {
  if (currentScreen != Screen::Main) {
    return;
  }
  
  if (millis() - lastRequest > 100) {
    sendCommand(Command::requestDistanceData);
    lastRequest = millis();
  }
}

// ====================== BROADCAST HANDLING ======================
void handlePairingBroadcast(const uint8_t* mac, const uint8_t* data, int len) {
    if (currentScreen != Screen::PairingMenu) {
      return;
    }

    char name[20] = "Unknown";
    if (len > 0) {
      int copyLen = min(len, (int)sizeof(name)-1);
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
    if (currentScreen != Screen::PairingMenu) {
      return;
    }
    
    unsigned long now = millis();
    discoveredRobots.erase(
        std::remove_if(discoveredRobots.begin(), discoveredRobots.end(),
            [now](const DiscoveredRobot& r){ return now - r.lastSeen > 7000; }),
        discoveredRobots.end()
    );
}

// ====================== ESP-NOW CALLBACK ======================
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len < 1) {
    return;
  }

  switch (static_cast<Command>(incomingData[0])) {
    case Command::distanceData:
      memcpy(distanceData, incomingData + 1, message::TOTAL_BYTES);
      printDistanceGrid();
      if (currentScreen == Screen::Main) {
        drawMainScreen();
      }
    break;
    case Command::broadcastPairing:
      handlePairingBroadcast(info->src_addr, incomingData + 1, len - 1);
    break;
    case Command::pairSuccess:
      uint8_t channel = 1;
      if (len >= 2) {
        channel = incomingData[1];
      }

      savePairedMac(info->src_addr);

      prefs.begin("robot", false);
      prefs.putUChar("channel", channel);
      prefs.end();

      WiFi.setChannel(channel, WIFI_SECOND_CHAN_NONE);

      currentScreen = Screen::Main;
      Serial.println("=== Pairing SUCCESS ===");

      addPairedPeer();
    break;
    case Command::pairReject:
      Serial.println("Pairing rejected by robot");
    break;
  }
}

// ====================== DRAWING ======================
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
  if (dist < 0) {
        return TFT_BLACK;
    }

  int16_t clamped = constrain(dist, MIN_DISTANCE, MAX_DISTANCE);

  uint8_t hue = map(clamped, MIN_DISTANCE, MAX_DISTANCE, 0, 240);

  return hslToColor565(hue / 360.0f, 1.0f, 0.5f);
}

void drawMainScreen() {
  spr.fillSprite(COLOR_BG);

  constexpr uint8_t grid       = message::GRID_SIZE;
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

    for (size_t i = 0; i < discoveredRobots.size() && i < 7; ++i) {  // max 7 na ekran
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

// ====================== INPUT ======================
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
    if (digitalRead(pins::KEY_B) == LOW) {
        currentScreen = Screen::Main;
        discoveredRobots.clear();
        selectedIndex = 0;
        drawMainScreen();
        Serial.println("Exited pairing menu");
    }

    if (needRedraw) {
        drawPairingMenu();
    }

    // update debouncing
    if (digitalRead(pins::J_UP) == LOW || digitalRead(pins::J_DOWN) == LOW ||
        digitalRead(pins::KEY_A) == LOW || digitalRead(pins::KEY_B) == LOW) {
        lastInput = millis();
    }
}

void handleMovement() {
  static unsigned long lastSend = 0;
  const unsigned long interval = 120;

  if (millis() - lastSend < interval) {
    return;
  }

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
  pinMode(pins::TFT_BL, OUTPUT);
  digitalWrite(pins::TFT_BL, HIGH);
  tft.init();
  tft.setRotation(lcdscreen::ROTATION);
  if (!spr.createSprite(lcdscreen::WIDTH, lcdscreen::HEIGHT)) {
    Serial.println("Sprite initialization failed!");
  }
  tft.fillScreen(TFT_RED);
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
  WiFi.setChannel(espnow::WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
}

void setup() {
  Serial.begin(115200);
  setupScreen();
  setupKeys();
  setupJoystick();
  setupESPNow();

  loadPairedMac();
  addPairedPeer();

  Serial.println("Initialization completed.");
}

void loop() {
  cleanupOldRobots();

    static unsigned long lastMenuPress = 0;
    if (digitalRead(pins::KEY_Y) == LOW && digitalRead(pins::KEY_B) == LOW) {
        if (millis() - lastMenuPress > 400) {
            if (currentScreen == Screen::Main) {
                currentScreen = Screen::PairingMenu;
                discoveredRobots.clear();
                selectedIndex = 0;
                Serial.println("Entered pairing menu");
            } else {
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

        static unsigned long lastDraw = 0;
        if (millis() - lastDraw > 250) {
            drawPairingMenu();
            lastDraw = millis();
        }
    } 
    else {
        handleMovement();
        requestDataFromRobot();
    }
}
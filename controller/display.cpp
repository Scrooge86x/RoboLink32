#include "display.h"
#include "comms.h"
#include <TFT_eSPI.h>
#include <SPI.h>

static TFT_eSPI tft = TFT_eSPI();
static TFT_eSprite spr = TFT_eSprite(&tft);

static uint16_t hslToColor565(float h, float s, float l) {
    float r, g, b;
    if (s == 0.0f) {
        r = g = b = l;
    } else {
        auto hue2rgb = [](float p, float q, float t) -> float {
            if (t < 0.0f) t += 1.0f;
            if (t > 1.0f) t -= 1.0f;
            if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
            if (t < 1.0f / 2.0f) return q;
            if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
            return p;
        };
        float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
        float p = 2.0f * l - q;
        r = hue2rgb(p, q, h + 1.0f / 3.0f);
        g = hue2rgb(p, q, h);
        b = hue2rgb(p, q, h - 1.0f / 3.0f);
    }
    uint8_t red   = static_cast<uint8_t>(r * 255.0f);
    uint8_t green = static_cast<uint8_t>(g * 255.0f);
    uint8_t blue  = static_cast<uint8_t>(b * 255.0f);
    return spr.color565(red, green, blue);
}

static uint16_t getHeatmapColor(uint16_t dist) {
    int16_t clamped = constrain(dist, heatmap::MIN_DISTANCE, heatmap::MAX_DISTANCE);
    uint8_t hue = map(clamped, heatmap::MIN_DISTANCE, heatmap::MAX_DISTANCE, 0, 240);
    return hslToColor565(hue / 360.0f, 1.0f, 0.5f);
}

void initDisplay() {
    pinMode(pins::TFT_BL_PIN, OUTPUT);
    digitalWrite(pins::TFT_BL_PIN, HIGH);
    tft.init();
    tft.setRotation(lcdscreen::ROTATION);
    if (!spr.createSprite(lcdscreen::WIDTH, lcdscreen::HEIGHT)) {
        Serial.println("[ERROR] Sprite creation failed!");
    }
}

void drawNotPaired() {
    spr.fillSprite(TFT_RED);
    spr.setTextColor(TFT_WHITE);
    spr.setTextSize(2);
    spr.setCursor(20, 120);
    spr.print("No robot paired");
    spr.pushSprite(0, 0);
}

static void drawMpuOverlay() {
    if (!displayMpu) {
        return;
    }
    spr.setTextColor(TFT_BLACK);
    spr.setTextSize(2);
    spr.setCursor(5, 5);
    spr.printf("YAW:   %.1f", yaw);
    spr.setCursor(5, 20);
    spr.printf("PITCH: %.1f", pitch);
    spr.setCursor(5, 35);
    spr.printf("ROLL:  %.1f", roll);
}

void drawHeatmap(const uint16_t distanceData[message::GRID_SIZE][message::GRID_SIZE]) {
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

    drawMpuOverlay();

    spr.pushSprite(0, 0);
}

void drawPairingMenu(const std::vector<DiscoveredRobot>& robots, int selectedIndex) {
    spr.fillSprite(TFT_BLACK);
    spr.setTextColor(TFT_WHITE);
    spr.setTextSize(2);
    spr.drawString("Select Robot:", 20, 10);

    if (robots.empty()) {
        spr.setTextColor(TFT_RED);
        spr.drawString("No robots found", 20, 80);
        spr.pushSprite(0, 0);
        return;
    }

    for (size_t i = 0; i < robots.size() && i < 7; ++i) {
        const auto& robot = robots[i];
        bool selected = (i == static_cast<size_t>(selectedIndex));
        uint16_t color = selected ? TFT_YELLOW : TFT_WHITE;
        uint16_t bg    = selected ? TFT_NAVY   : TFT_BLACK;
        char line[20];
        snprintf(line, sizeof(line), "%s", robot.name);
        spr.setTextColor(color, bg);
        spr.drawString(line, 20, 50 + i * 28);
    }
    spr.pushSprite(0, 0);
}
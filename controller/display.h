#ifndef DISPLAY_H
#define DISPLAY_H

#include <cstdint>
#include <vector>
#include "config.h"

struct DiscoveredRobot {
    uint8_t mac[6];
    char name[19];
    unsigned long lastSeen;
};

void initDisplay();

void drawNotPaired();
void drawHeatmap(const uint16_t distanceData[message::GRID_SIZE][message::GRID_SIZE]);
void drawPairingMenu(const std::vector<DiscoveredRobot>& robots, int selectedIndex);

#endif // DISPLAY_H
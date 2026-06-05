#ifndef COMMS_H
#define COMMS_H

#include "commands.h"
#include "config.h"
#include <esp_now.h>

extern uint16_t distanceData[message::GRID_SIZE][message::GRID_SIZE];
extern unsigned long lastSeen;

void sendCommand(Command cmd);
void sendCommandTo(const uint8_t* mac, Command cmd);
void requestDataFromRobot();
void setupESPNow();
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len);

#endif
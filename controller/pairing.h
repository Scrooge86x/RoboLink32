#ifndef PAIRING_H
#define PAIRING_H

#include <cstdint>
#include <vector>
#include "commands.h"
#include "display.h"

enum class Screen {
    Main,
    PairingMenu
};

void initPairing();

void updatePairing();
void handlePairingReceive(const uint8_t* srcMac, const uint8_t* data, int len);
void handlePairingInput();
void enterPairingMode();
void exitPairingMode();

bool isPairingMode();
bool isPaired();

const uint8_t* getPairedMac();
uint8_t getPairedChannel();

void sendCommandToPaired(Command cmd);

void printMac(const uint8_t* mac);

#endif // PAIRING_H
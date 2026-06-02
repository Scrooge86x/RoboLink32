#ifndef PAIRING_H
#define PAIRING_H

#include <cstdint>

namespace pairing {

void setup();
void start();
void update();
void onDataRecv(const uint8_t* srcMac, const uint8_t* data, int len);

bool isActive();
bool isPaired();
const uint8_t* getPairedMac();

} // pairing

#endif // PAIRING_H

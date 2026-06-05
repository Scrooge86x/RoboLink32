#ifndef COMMANDS_H
#define COMMANDS_H

#include <cstdint>

enum class Command : uint8_t {
    forwardSlow,
    forwardFast,
    backwardSlow,
    backwardFast,
    leftSlow,
    leftFast,
    rightSlow,
    rightFast,

    broadcastPairing,
    pairRequest,
    pairSuccess,

    requestDistanceData,
    distanceData,
};

#endif // COMMANDS_H

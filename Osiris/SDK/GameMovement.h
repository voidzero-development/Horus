#pragma once

#include "VirtualMethod.h"

class Entity;
class MoveData;

class GameMovement {
public:
    VIRTUAL_METHOD_V(void, processMovement, 1, (Entity* localPlayer, MoveData* moveData), (this, localPlayer, moveData))
    VIRTUAL_METHOD(void, startTrackPredictionErrors, 3, (Entity* localPlayer), (this, localPlayer))
    VIRTUAL_METHOD(void, finishTrackPredictionErrors, 4, (Entity* localPlayer), (this, localPlayer))
    VIRTUAL_METHOD(Vector&, getPlayerViewOffset, 8, (bool ducked), (this, ducked))
};

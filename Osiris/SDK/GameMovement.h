#pragma once

#include "VirtualMethod.h"

class Entity;
class MoveData;

class GameMovement {
public:
    VIRTUAL_METHOD_V(void, processMovement, 1, (Entity* localPlayer, MoveData* moveData), (this, localPlayer, moveData))
    VIRTUAL_METHOD(void, start_track_prediction_errors, 3, (Entity* localPlayer), (this, localPlayer))
    VIRTUAL_METHOD(void, finish_track_prediction_errors, 4, (Entity* localPlayer), (this, localPlayer))
    VIRTUAL_METHOD(Vector&, getPlayerViewOffset, 8, (bool ducked), (this, ducked))
};

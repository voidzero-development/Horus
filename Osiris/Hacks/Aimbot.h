#pragma once

#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"
#include "../SDK/Angle.h"
#include "../SDK/Entity.h"
#include "../SDK/UserCmd.h"
#include "../SDK/Vector.h"
#include "../SDK/WeaponId.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/PhysicsSurfaceProps.h"
#include "../SDK/WeaponData.h"
#include "../SDK/StudioRender.h"
#include "../SDK/ModelInfo.h"

class Entity;
struct UserCmd;
struct Vector;

namespace Aimbot
{
    bool hitChance(Entity* localPlayer, Entity* entity, Entity* weaponData, const Vector& destination, const UserCmd* cmd, const int hitChance) noexcept;
    Vector calculateRelativeAngle(const Vector& source, const Vector& destination, const Vector& viewAngles) noexcept;
    float canScan(Entity* entity, const Vector& destination, const WeaponInfo* weaponData, bool allowFriendlyFire) noexcept;
    std::vector<Vector> multiPoint(Entity* entity, matrix3x4 matrix[256], StudioHdr* hdr, int iHitbox, int weaponClass, int multiPoint) noexcept;
}

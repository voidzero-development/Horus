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
    Vector calculateRelativeAngle(const Vector& source, const Vector& destination, const Vector& viewAngles) noexcept;
}

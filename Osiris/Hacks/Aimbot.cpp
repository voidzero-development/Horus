#include "Aimbot.h"
#include "Backtrack.h"
#include "Misc.h"

Vector Aimbot::calculateRelativeAngle(const Vector& source, const Vector& destination, const Vector& viewAngles) noexcept
{
    return ((destination - source).toAngle() - viewAngles).normalize();
}
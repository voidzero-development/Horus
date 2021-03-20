#pragma once

class Entity;
struct UserCmd;
struct Vector;

namespace Aimbot
{
    bool hitChance(Entity* localPlayer, Entity* entity, Entity* weaponData, const Vector& destination, const UserCmd* cmd, const int hitChance) noexcept;
    Vector calculateRelativeAngle(const Vector& source, const Vector& destination, const Vector& viewAngles) noexcept;
    void autoStop(UserCmd* cmd) noexcept;
    void run(UserCmd*) noexcept;

    void updateInput() noexcept;
}

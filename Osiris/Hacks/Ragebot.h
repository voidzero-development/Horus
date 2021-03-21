#pragma once

class Entity;
struct UserCmd;
struct Vector;

namespace Ragebot
{
    void autoStop(UserCmd* cmd) noexcept;
    void run(UserCmd*) noexcept;

    void updateInput() noexcept;
}
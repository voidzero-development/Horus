#pragma once

class Entity;
struct UserCmd;
struct Vector;

namespace Legitbot
{
    static float lastKillTime{ 0 };

    void handleKill(GameEvent& event) noexcept;
    void autoStop(UserCmd* cmd) noexcept;
    void run(UserCmd*) noexcept;

    void updateInput() noexcept;
}
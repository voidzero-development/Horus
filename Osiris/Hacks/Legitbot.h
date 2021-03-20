#pragma once

class Entity;
struct UserCmd;
struct Vector;

namespace Legitbot
{
    //void autoStop(UserCmd* cmd) noexcept;
    void run(UserCmd*) noexcept;

    void updateInput() noexcept;
}
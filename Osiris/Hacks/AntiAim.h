#pragma once

#include "../ConfigStructs.h"

struct UserCmd;
struct Vector;

#define OSIRIS_ANTIAIM() true

namespace AntiAim
{
    bool fakeDucking = false;

    void run(UserCmd* cmd, const Vector& previousViewAngles, const Vector& currentViewAngles, bool& sendPacket) noexcept;
    void fakeLag(UserCmd* cmd, bool& sendPacket) noexcept;
    void fakeDuck(UserCmd* cmd) noexcept;

    // GUI
    void menuBarItem() noexcept;
    void tabItem() noexcept;
    void drawGUI(bool contentOnly) noexcept;

    // Config
    json toJson() noexcept;
    void fromJson(const json& j) noexcept;
    void resetConfig() noexcept;
}

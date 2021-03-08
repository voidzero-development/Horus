#define NOMINMAX

#include "AntiAim.h"
#include "../Interfaces.h"
#include "../SDK/Engine.h"
#include "../SDK/Entity.h"
#include "../SDK/EntityList.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/NetworkChannel.h"
#include "../SDK/UserCmd.h"

#if OSIRIS_ANTIAIM()

struct AntiAimConfig {
    bool enabled = false;
    int pitchAngle = 0;
    int yawOffset = 0;
} antiAimConfig;

bool isLbyUpdating() noexcept
{
    static float update = 0.f;

    if (!(localPlayer->flags() & 1))
        return false;

    if (localPlayer->velocity().length2D() > 0.1f)
    {
        update = memory->globalVars->serverTime() + 0.22f;
    }
    if (update < memory->globalVars->serverTime())
    {
        update = memory->globalVars->serverTime() + 1.1f;
        return true;
    }
    return false;
}

bool autoDir(Entity* entity, Vector eye) noexcept
{
    constexpr float maxRange{ 8192.0f };

    eye.x = 0;
    Vector eyeAnglesLeft45 = eye;
    Vector eyeAnglesRight45 = eye;
    eyeAnglesLeft45.y += 45.f;
    eyeAnglesRight45.y -= 45.f;

    Vector viewAnglesLeft45{ cos(degreesToRadians(eyeAnglesLeft45.x)) * cos(degreesToRadians(eyeAnglesLeft45.y)) * maxRange,
               cos(degreesToRadians(eyeAnglesLeft45.x)) * sin(degreesToRadians(eyeAnglesLeft45.y)) * maxRange,
              -sin(degreesToRadians(eyeAnglesLeft45.x)) * maxRange };

    Vector viewAnglesRight45{ cos(degreesToRadians(eyeAnglesRight45.x)) * cos(degreesToRadians(eyeAnglesRight45.y)) * maxRange,
                       cos(degreesToRadians(eyeAnglesRight45.x)) * sin(degreesToRadians(eyeAnglesRight45.y)) * maxRange,
                      -sin(degreesToRadians(eyeAnglesRight45.x)) * maxRange };

    static Trace traceLeft45;
    static Trace traceRight45;

    Vector headPosition{ localPlayer->getBonePosition(8) };

    interfaces->engineTrace->traceRay({ headPosition, headPosition + viewAnglesLeft45 }, 0x4600400B, { entity }, traceLeft45);
    interfaces->engineTrace->traceRay({ headPosition, headPosition + viewAnglesRight45 }, 0x4600400B, { entity }, traceRight45);

    float distanceLeft45 = (float)sqrt(pow(headPosition.x - traceRight45.endpos.x, 2) + pow(headPosition.y - traceRight45.endpos.y, 2) + pow(headPosition.z - traceRight45.endpos.z, 2));
    float distanceRight45 = (float)sqrt(pow(headPosition.x - traceLeft45.endpos.x, 2) + pow(headPosition.y - traceLeft45.endpos.y, 2) + pow(headPosition.z - traceLeft45.endpos.z, 2));

    float minDistance = std::min(distanceLeft45, distanceRight45);

    if (distanceLeft45 == minDistance) {
        return false;
    }
    return true;
}

void AntiAim::run(UserCmd* cmd, const Vector& previousViewAngles, const Vector& currentViewAngles, bool& sendPacket) noexcept
{
    bool lby = isLbyUpdating();
    bool invert = autoDir(localPlayer.get(), cmd->viewangles);
        
    if (cmd->buttons & (UserCmd::IN_ATTACK | UserCmd::IN_ATTACK2 | UserCmd::IN_USE))
        return;

    if (antiAimConfig.enabled) {
        switch (antiAimConfig.pitchAngle) {
        case 0: //Off
            break;
        case 1: //Down
            cmd->viewangles.x = 89.f;
            break;
        case 2: //Zero
            cmd->viewangles.x = 0.f;
            break;
        case 3: //Up
            cmd->viewangles.x = -89.f;
            break;
        }

        switch (antiAimConfig.yawOffset) {
        case 0: //Off
            break;
        case 1: //Back
            cmd->viewangles.y += 180.f;
            invert ^= 1;
            break;
        }

        if (lby) {
            sendPacket = false;
            invert ? cmd->viewangles.y -= 119.95f : cmd->viewangles.y += 119.95f;
            return;
        }

        if (!sendPacket) {
            invert ? cmd->viewangles.y += 58.f : cmd->viewangles.y -= 58.f;
        }
    }
}

static bool antiAimOpen = false;

void AntiAim::menuBarItem() noexcept
{
    if (ImGui::MenuItem("Anti aim")) {
        antiAimOpen = true;
        ImGui::SetWindowFocus("Anti aim");
        ImGui::SetWindowPos("Anti aim", { 100.0f, 100.0f });
    }
}

void AntiAim::tabItem() noexcept
{
    if (ImGui::BeginTabItem("Anti aim")) {
        drawGUI(true);
        ImGui::EndTabItem();
    }
}

void AntiAim::drawGUI(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!antiAimOpen)
            return;
        ImGui::SetNextWindowSize({ 0.0f, 0.0f });
        ImGui::Begin("Anti aim", &antiAimOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    }
    ImGui::Checkbox("Enabled", &antiAimConfig.enabled);
    ImGui::Combo("Pitch angle", &antiAimConfig.pitchAngle, "Off\0Down\0Zero\0Up\0");
    ImGui::Combo("Yaw offset", &antiAimConfig.yawOffset, "Off\0Back\0");
    if (!contentOnly)
        ImGui::End();
}

static void to_json(json& j, const AntiAimConfig& o, const AntiAimConfig& dummy = {})
{
    WRITE("Enabled", enabled);
    WRITE("Pitch angle", pitchAngle);
    WRITE("Yaw offset", yawOffset);
}

json AntiAim::toJson() noexcept
{
    json j;
    to_json(j, antiAimConfig);
    return j;
}

static void from_json(const json& j, AntiAimConfig& a)
{
    read(j, "Enabled", a.enabled);
    read(j, "Pitch angle", a.pitchAngle);
    read(j, "Yaw offset", a.yawOffset);
}

void AntiAim::fromJson(const json& j) noexcept
{
    from_json(j, antiAimConfig);
}

void AntiAim::resetConfig() noexcept
{
    antiAimConfig = { };
}

#else

namespace AntiAim
{
    void run(UserCmd*, const Vector&, const Vector&, bool&) noexcept {}

    // GUI
    void menuBarItem() noexcept {}
    void tabItem() noexcept {}
    void drawGUI(bool contentOnly) noexcept {}

    // Config
    json toJson() noexcept { return {}; }
    void fromJson(const json& j) noexcept {}
    void resetConfig() noexcept {}
}

#endif

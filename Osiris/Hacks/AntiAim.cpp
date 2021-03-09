#define NOMINMAX

#include "Aimbot.h"
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
    bool lbyBreak = false;
    int pitchAngle = 0;
    int yawOffset = 0;

    bool fakeLag = false;
    int flLimit = 1;
    
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

bool shouldRun(UserCmd* cmd) noexcept
{
    if (!localPlayer || !localPlayer->isAlive())
        return false;

    if ((cmd->buttons & (UserCmd::IN_USE)))
        return false;

    if (localPlayer->moveType() == MoveType::LADDER || localPlayer->moveType() == MoveType::NOCLIP)
        return false;

    auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon || !activeWeapon->clip())
        return true;

    if (activeWeapon->isThrowing())
        return false;

    if (localPlayer->shotsFired() > 0 && !activeWeapon->isFullAuto() || localPlayer->waitForNoAttack())
        return true;

    if (localPlayer->nextAttack() > memory->globalVars->serverTime())
        return true;

    if (activeWeapon->nextPrimaryAttack() > memory->globalVars->serverTime())
        return true;

    if (activeWeapon->nextSecondaryAttack() > memory->globalVars->serverTime())
        return true;

    if (localPlayer->nextAttack() <= memory->globalVars->serverTime() && (cmd->buttons & (UserCmd::IN_ATTACK)))
        return false;

    if (activeWeapon->nextPrimaryAttack() <= memory->globalVars->serverTime() && (cmd->buttons & (UserCmd::IN_ATTACK)))
        return false;

    if (activeWeapon->itemDefinitionIndex2() == WeaponId::Revolver && activeWeapon->readyTime() > memory->globalVars->serverTime())
        return true;

    if ((activeWeapon->itemDefinitionIndex2() == WeaponId::Famas || activeWeapon->itemDefinitionIndex2() == WeaponId::Glock) && activeWeapon->burstMode() && activeWeapon->burstShotRemaining() > 0)
        return true;

    return true;
}

void AntiAim::run(UserCmd* cmd, const Vector& previousViewAngles, const Vector& currentViewAngles, bool& sendPacket) noexcept
{
    if (!shouldRun(cmd))
        return;

    Vector angle{ };
    auto bestFov = 255.f;
    Vector bestTarget{ };
    for (int i = 1; i <= interfaces->engine->getMaxClients(); i++) {
        auto entity = interfaces->entityList->getEntity(i);
        if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive()
            || !entity->isOtherEnemy(localPlayer.get()))
            continue;

        const auto headPosition = entity->getBonePosition(8);
        auto angle = Aimbot::calculateRelativeAngle(localPlayer->getBonePosition(8), headPosition, cmd->viewangles);
        bool clamped{ false };

        const auto fov = std::hypot(angle.x, angle.y);
        if (fov > bestFov)
            continue;

        if (fov < bestFov) {
            bestFov = fov;
            bestTarget = headPosition;
        }
    }

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

        float yawOffset{ 0.f };
        switch (antiAimConfig.yawOffset) {
        case 0: //Off
            break;
        case 1: //Back
            if (bestTarget.notNull()) {
                auto angle = Aimbot::calculateRelativeAngle(localPlayer->getBonePosition(8), bestTarget, cmd->viewangles);
                cmd->viewangles.y += angle.y;
            }
            yawOffset += 180.f;
            break;
        }

        bool lby = isLbyUpdating();
        bool invert = autoDir(localPlayer.get(), cmd->viewangles);
        if (fabsf(yawOffset) > 90.f) {
            cmd->buttons ^= UserCmd::IN_FORWARD | UserCmd::IN_BACK | UserCmd::IN_MOVELEFT | UserCmd::IN_MOVERIGHT;
            invert ^= 1;
        }

        cmd->viewangles.y += yawOffset;

        if (lby) {
            sendPacket = false;
            if (antiAimConfig.lbyBreak)
                invert ? cmd->viewangles.y -= 119.95f : cmd->viewangles.y += 119.95f;
            return;
        }

        if (!sendPacket) {
            invert ? cmd->viewangles.y += localPlayer->getMaxDesyncAngle() * 2 : cmd->viewangles.y -= localPlayer->getMaxDesyncAngle() * 2;
        }
    }
}

void AntiAim::fakeLag(UserCmd* cmd, bool& sendPacket) noexcept
{
    auto chokedPackets = 0;

    if (!localPlayer->isAlive())
        return;

    chokedPackets = antiAimConfig.enabled ? 1 : 0;

    if (antiAimConfig.fakeLag)
        chokedPackets = std::clamp(antiAimConfig.flLimit, 1, 14);

    if (!shouldRun(cmd))
        return;

    sendPacket = interfaces->engine->getNetworkChannel()->chokedPackets >= chokedPackets;
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
    ImGui::Checkbox("Extend LBY", &antiAimConfig.lbyBreak);
    ImGui::Combo("Pitch angle", &antiAimConfig.pitchAngle, "Off\0Down\0Zero\0Up\0");
    ImGui::Combo("Yaw offset", &antiAimConfig.yawOffset, "Off\0Back\0");
    ImGui::Checkbox("Fake lag", &antiAimConfig.fakeLag);
    ImGui::SliderInt("Limit", &antiAimConfig.flLimit, 1, 14, "%d");
    if (!contentOnly)
        ImGui::End();
}

static void to_json(json& j, const AntiAimConfig& o, const AntiAimConfig& dummy = {})
{
    WRITE("Enabled", enabled);
    WRITE("Extend LBY", lbyBreak);
    WRITE("Pitch angle", pitchAngle);
    WRITE("Yaw offset", yawOffset);
    WRITE("Fake lag", fakeLag);
    WRITE("Limit", flLimit);
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
    read(j, "Extend LBY", a.lbyBreak);
    read(j, "Pitch angle", a.pitchAngle);
    read(j, "Yaw offset", a.yawOffset);
    read(j, "Fake lag", a.fakeLag);
    read(j, "Limit", a.flLimit);
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

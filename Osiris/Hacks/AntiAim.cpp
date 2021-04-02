#define NOMINMAX

#include "Aimbot.h"
#include "AntiAim.h"
#include "Animations.h"
#include "../Interfaces.h"
#include "../imguiCustom.h"
#include "../SDK/Engine.h"
#include "../SDK/Entity.h"
#include "../SDK/EntityList.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/NetworkChannel.h"
#include "../SDK/UserCmd.h"

#if OSIRIS_ANTIAIM()

struct AntiAimConfig {
    bool enabled = false;
    AntiAimDisablers aaDisablers;
    int extendMode = 0;
    int desyncAmount = 100;
    int pitchAngle = 0;
    int yawOffset = 0;
    float yawJitter = 0;
    int jitterMode = 0;
    bool atTarget = false;

    bool fakeLag = false;
    FakeLagDisablers flDisablers;
    int flMode = 0;
    int flLimit = 1;
    FakeLagTriggers flTriggers;
    int flTriggerLimit = 1;
    
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

bool inAttack(UserCmd* cmd) noexcept
{
    if (!(cmd->buttons & (UserCmd::IN_ATTACK)))
        return false;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon || !activeWeapon->clip())
        return false;

    if (activeWeapon->isGrenade())
        return false;

    if (localPlayer->nextAttack() > memory->globalVars->serverTime())
        return false;

    if (activeWeapon->nextPrimaryAttack() > memory->globalVars->serverTime())
        return false;

    if (localPlayer->shotsFired() > 0 && !activeWeapon->isFullAuto())
        return false;

    return true;
}

bool inAttack2(UserCmd* cmd) noexcept
{
    if (!(cmd->buttons & (UserCmd::IN_ATTACK2)))
        return false;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon || !activeWeapon->clip())
        return false;

    if (!activeWeapon->isKnife() && activeWeapon->itemDefinitionIndex2() != WeaponId::Revolver)
        return false;

    if (localPlayer->nextAttack() > memory->globalVars->serverTime())
        return false;

    if (activeWeapon->nextSecondaryAttack() > memory->globalVars->serverTime())
        return false;

    if (localPlayer->shotsFired() > 0 && !activeWeapon->isFullAuto())
        return false;

    return true;
}

bool didShoot(UserCmd* cmd) noexcept
{
    if (!localPlayer || localPlayer->nextAttack() > memory->globalVars->serverTime() || localPlayer->isDefusing() || localPlayer->waitForNoAttack())
        return false;

    if ((*memory->gameRules)->freezePeriod())
        return false;

    if (inAttack(cmd))
        return true;

    if (inAttack2(cmd))
        return true;

    return false;
}

void AntiAim::run(UserCmd* cmd, const Vector& previousViewAngles, const Vector& currentViewAngles, bool& sendPacket) noexcept
{
    bool lby = isLbyUpdating();
    Animations::data.lby = antiAimConfig.extendMode == 0 ? lby : 0;

    if (!localPlayer->isAlive())
        return;

    if (localPlayer->moveType() == MoveType::LADDER || localPlayer->moveType() == MoveType::NOCLIP)
        return;

    if ((cmd->buttons & (UserCmd::IN_USE)))
        return;

    if ((antiAimConfig.aaDisablers.enabled[0] //On freeze period disabler
        && (*memory->gameRules)->freezePeriod()))
        return;

    auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon)
        return;

    if (activeWeapon->isThrowing())
        return;

    if (didShoot(cmd))
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
        static bool flipJitter{ false };

        if (bestTarget.notNull() && antiAimConfig.yawOffset && antiAimConfig.atTarget) {
            auto angle = Aimbot::calculateRelativeAngle(localPlayer->getBonePosition(8), bestTarget, cmd->viewangles);
            cmd->viewangles.y += angle.y;
        }

        switch (antiAimConfig.yawOffset) {
        case 0: //Off
            break;
        case 1: //Back
            yawOffset += 180.f;
            break;
        case 2: //Forward jitter
            switch (antiAimConfig.jitterMode) {
            case 0: //Flip
                yawOffset = flipJitter ? antiAimConfig.yawJitter : -antiAimConfig.yawJitter;
                break;
            case 1: //Random
                yawOffset = randomFloat(antiAimConfig.yawJitter, -antiAimConfig.yawJitter);
                break;
            }
            break;
        case 3: //Back jitter
            switch (antiAimConfig.jitterMode) {
            case 0: //Flip
                yawOffset = flipJitter ? antiAimConfig.yawJitter : -antiAimConfig.yawJitter;
                break;
            case 1: //Random
                yawOffset = randomFloat(antiAimConfig.yawJitter, -antiAimConfig.yawJitter);
                break;
            }
            yawOffset += 180.f;
            break;
        }

        if (sendPacket)
            flipJitter ^= 1;

        bool invert = autoDir(localPlayer.get(), cmd->viewangles);
        if (fabsf(yawOffset + angle.y) > 90.f)
            invert ^= 1;

        cmd->viewangles.y += yawOffset;

        if (lby && antiAimConfig.extendMode == 0) {
            sendPacket = false;
            invert ? cmd->viewangles.y -= 119.95f : cmd->viewangles.y += 119.95f;
            return;
        }

        if (!sendPacket) 
        {
            if (((*memory->gameRules)->freezePeriod()))
                return;

            invert ? cmd->viewangles.y += (localPlayer->getMaxDesyncAngle() * 2) * ((antiAimConfig.desyncAmount / 2 + 50) / 100.f) : cmd->viewangles.y -= (localPlayer->getMaxDesyncAngle() * 2) * ((antiAimConfig.desyncAmount / 2 + 50) / 100.f);
        }

        if (antiAimConfig.extendMode == 1)
        {
            if (fabsf(cmd->sidemove) < 5.0f) {
                if (cmd->buttons & UserCmd::IN_DUCK)
                    cmd->sidemove = cmd->tickCount & 1 ? 3.25f : -3.25f;
                else
                    cmd->sidemove = cmd->tickCount & 1 ? 1.1f : -1.1f;
            }
        }
    }
}

void AntiAim::fakeLag(UserCmd* cmd, bool& sendPacket) noexcept
{
    auto chokedPackets = 0;

    chokedPackets = antiAimConfig.enabled ? 1 : 0;

    auto limit = std::clamp(antiAimConfig.flLimit, 1, (*memory->gameRules)->isValveDS() ? 6 : 14);
    auto triggerLimit = std::clamp(antiAimConfig.flTriggerLimit, 1, (*memory->gameRules)->isValveDS() ? 6 : 14);

    if (!localPlayer->isAlive())
        return;

    auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon)
        return;

    if (antiAimConfig.fakeLag) {
        switch (antiAimConfig.flMode) {
        case 0: //Static
            chokedPackets = limit;
            break;
        case 1: //Adaptive
            chokedPackets = std::clamp(static_cast<int>(std::ceilf(64 / (localPlayer->velocity().length() * memory->globalVars->intervalPerTick))), 1, limit);
            break;
        }

        for (int i = 1; i <= interfaces->engine->getMaxClients(); i++) {
            auto entity = interfaces->entityList->getEntity(i);
            if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive()
                || !entity->isOtherEnemy(localPlayer.get()))
                continue;

            if ((antiAimConfig.flTriggers.enabled[0] //On visible trigger
                && entity->isVisible(localPlayer->getBonePosition(8))))
                chokedPackets = triggerLimit;
        }

        if ((antiAimConfig.flDisablers.enabled[0] //On shot disabler
            && didShoot(cmd)))
            chokedPackets = antiAimConfig.enabled ? 1 : 0;

        if ((antiAimConfig.flDisablers.enabled[1] //On use disabler
            && (cmd->buttons & (UserCmd::IN_USE))))
            chokedPackets = antiAimConfig.enabled ? 1 : 0;

        if ((antiAimConfig.flDisablers.enabled[2] //On grenade throw disabler
            && activeWeapon->isThrowing()))
            chokedPackets = antiAimConfig.enabled ? 1 : 0;
    }
    else {
        if (didShoot(cmd))
            return;
    }

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
    ImGui::PushID("Anti aim disablers");
    ImGui::multiCombo("Disablers", antiAimConfig.aaDisablers.aaDisablersText.data(), antiAimConfig.aaDisablers.enabled, antiAimConfig.aaDisablers.aaDisablersText.size());
    ImGui::PopID();
    ImGui::Combo("Extend mode", &antiAimConfig.extendMode, "Break LBY\0Micromovement\0");
    ImGui::SliderInt("Desync amount", &antiAimConfig.desyncAmount, 1, 100, "%d%%");
    ImGui::Combo("Pitch angle", &antiAimConfig.pitchAngle, "Off\0Down\0Zero\0Up\0");
    ImGui::Combo("Yaw offset", &antiAimConfig.yawOffset, "Off\0Back\0Forward jitter\0Back jitter\0");
    if (antiAimConfig.yawOffset >= 2) {
        ImGui::SliderFloat("Yaw jitter", &antiAimConfig.yawJitter, -90.f, 90.f, "%.2f");
        ImGui::Combo("Jitter mode", &antiAimConfig.jitterMode, "Flip\0Random\0");
    }
    if (antiAimConfig.yawOffset)
        ImGui::Checkbox("At target", &antiAimConfig.atTarget);
    ImGui::Separator();
    ImGui::Checkbox("Fake lag", &antiAimConfig.fakeLag);
    ImGui::multiCombo("Disablers", antiAimConfig.flDisablers.flDisablersText.data(), antiAimConfig.flDisablers.enabled, antiAimConfig.flDisablers.flDisablersText.size());
    ImGui::Combo("Mode", &antiAimConfig.flMode, "Static\0Adaptive\0");
    ImGui::SliderInt("Limit", &antiAimConfig.flLimit, 1, 14, "%d");
    ImGui::multiCombo("Triggers", antiAimConfig.flTriggers.flTriggersText.data(), antiAimConfig.flTriggers.enabled, antiAimConfig.flTriggers.flTriggersText.size());
    ImGui::SliderInt("Trigger limit", &antiAimConfig.flTriggerLimit, 1, 14, "%d");
    if (!contentOnly)
        ImGui::End();
}

static void to_json(json& j, const AntiAimDisablers& o, const AntiAimDisablers& dummy = {})
{
    WRITE("On freeze period", enabled[0]);
}

static void to_json(json& j, const FakeLagDisablers& o, const FakeLagDisablers& dummy = {})
{
    WRITE("On shot", enabled[0]);
    WRITE("On use", enabled[1]);
    WRITE("On grenade throw", enabled[1]);
}

static void to_json(json& j, const FakeLagTriggers& o, const FakeLagTriggers& dummy = {})
{
    WRITE("On visible", enabled[0]);
}

static void to_json(json& j, const AntiAimConfig& o, const AntiAimConfig& dummy = {})
{
    WRITE("Enabled", enabled);
    WRITE("Extend mode", extendMode);
    WRITE("Desync amount", desyncAmount);
    WRITE("Pitch angle", pitchAngle);
    WRITE("Yaw offset", yawOffset);
    WRITE("Yaw jitter", yawJitter);
    WRITE("Jitter mode", jitterMode);
    WRITE("At target", atTarget);
    WRITE("Fake lag", fakeLag);
    WRITE("Fake lag disablers", flDisablers);
    WRITE("Fake lag mode", flMode);
    WRITE("Fake lag limit", flLimit);
    WRITE("Fake lag triggers", flTriggers);
    WRITE("Fake lag trigger limit", flTriggerLimit);
}

json AntiAim::toJson() noexcept
{
    json j;
    to_json(j, antiAimConfig);
    return j;
}

static void from_json(const json& j, AntiAimDisablers& ad)
{
    read(j, "On freeze period", ad.enabled[0]);
}

static void from_json(const json& j, FakeLagDisablers& fd)
{
    read(j, "On shot", fd.enabled[0]);
    read(j, "On use", fd.enabled[1]);
    read(j, "On grenade throw", fd.enabled[1]);
}

static void from_json(const json& j, FakeLagTriggers& ft)
{
    read(j, "On visible", ft.enabled[0]);
}

static void from_json(const json& j, AntiAimConfig& a)
{
    read(j, "Enabled", a.enabled);
    read(j, "Extend mode", a.extendMode);
    read(j, "Desync amount", a.desyncAmount);
    read(j, "Pitch angle", a.pitchAngle);
    read(j, "Yaw offset", a.yawOffset);
    read(j, "Yaw jitter", a.yawJitter);
    read(j, "Jitter mode", a.jitterMode);
    read(j, "At target", a.atTarget);
    read(j, "Fake lag", a.fakeLag);
    read<value_t::object>(j, "Fake lag disablers", a.flDisablers);
    read(j, "Fake lag mode", a.flMode);
    read(j, "Fake lag limit", a.flLimit);
    read<value_t::object>(j, "Fake lag triggers", a.flTriggers);
    read(j, "Fake lag trigger limit", a.flTriggerLimit);
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

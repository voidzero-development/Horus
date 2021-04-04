#include "Aimbot.h"
#include "Animations.h"
#include "Backtrack.h"
#include "Legitbot.h"
#include "Misc.h"
#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"
#include "../SDK/Angle.h"
#include "../SDK/Entity.h"
#include "../SDK/UserCmd.h"
#include "../SDK/Vector.h"
#include "../SDK/WeaponId.h"
#include "../SDK/GameEvent.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/PhysicsSurfaceProps.h"
#include "../SDK/WeaponData.h"
#include "../SDK/StudioRender.h"
#include "../SDK/ModelInfo.h"

static bool keyPressed = false;

void Legitbot::updateInput() noexcept
{
    if (config->legitbotKeyMode == 0)
        keyPressed = config->legitbotKey.isDown();
    if (config->legitbotKeyMode == 1 && config->legitbotKey.isPressed())
        keyPressed = !keyPressed;
}

void Legitbot::handleKill(GameEvent& event) noexcept
{
    if (!localPlayer)
        return;

    if (const auto localUserId = localPlayer->getUserId(); event.getInt("attacker") != localUserId || event.getInt("userid") == localUserId)
        return;

    lastKillTime = memory->globalVars->realtime;
    return;
}

static std::array<bool, 7> shouldRunAutoStop;

void Legitbot::autoStop(UserCmd* cmd) noexcept
{
    if (!localPlayer || !localPlayer->isAlive())
        return;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon || !activeWeapon->clip())
        return;

    auto weaponIndex = getWeaponIndex(activeWeapon->itemDefinitionIndex2());
    if (!weaponIndex)
        return;

    auto weaponClass = getWeaponClass(activeWeapon->itemDefinitionIndex2());
    if (!config->legitbot[weaponClass].enabled)
        weaponClass = 0;

    const auto& cfg = config->legitbot[weaponClass];

    if (!cfg.autoStop || !shouldRunAutoStop.at(weaponClass))
        return;

    if (cfg.silent && activeWeapon->nextPrimaryAttack() > memory->globalVars->serverTime())
        return;

    if (!cfg.ignoreFlash && localPlayer->isFlashed())
        return;

    if (config->legitbotOnKey && !keyPressed)
        return;

    if (cfg.enabled && (cmd->buttons & UserCmd::IN_ATTACK || cfg.autoShot))
    {
        if (cfg.scopedOnly && activeWeapon->isSniperRifle() && !localPlayer->isScoped())
            return;

        const auto weaponData = activeWeapon->getWeaponData();
        if (!weaponData)
            return;

        const float maxSpeed = (localPlayer->isScoped() ? weaponData->maxSpeedAlt : weaponData->maxSpeed) / 3;

        if (cmd->forwardmove && cmd->sidemove) {
            const float maxSpeedRoot = maxSpeed * static_cast<float>(M_SQRT1_2);
            cmd->forwardmove = cmd->forwardmove < 0.0f ? -maxSpeedRoot : maxSpeedRoot;
            cmd->sidemove = cmd->sidemove < 0.0f ? -maxSpeedRoot : maxSpeedRoot;
        }
        else if (cmd->forwardmove) {
            cmd->forwardmove = cmd->forwardmove < 0.0f ? -maxSpeed : maxSpeed;
        }
        else if (cmd->sidemove) {
            cmd->sidemove = cmd->sidemove < 0.0f ? -maxSpeed : maxSpeed;
        }
    }
    shouldRunAutoStop.at(weaponClass) = false;
}

void Legitbot::run(UserCmd* cmd) noexcept
{
    if (!localPlayer || localPlayer->nextAttack() > memory->globalVars->serverTime() || localPlayer->isDefusing() || localPlayer->waitForNoAttack())
        return;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon || !activeWeapon->clip())
        return;

    if (localPlayer->shotsFired() > 0 && !activeWeapon->isFullAuto())
        return;

    auto weaponIndex = getWeaponIndex(activeWeapon->itemDefinitionIndex2());
    if (!weaponIndex)
        return;

    auto weaponClass = getWeaponClass(activeWeapon->itemDefinitionIndex2());
    if (!config->legitbot[weaponClass].enabled)
        weaponClass = 0;

    const auto& cfg = config->legitbot[weaponClass];

    if (cfg.silent && activeWeapon->nextPrimaryAttack() > memory->globalVars->serverTime())
        return;

    if (!cfg.ignoreFlash && localPlayer->isFlashed())
        return;

    const auto now = memory->globalVars->realtime;

    if (lastKillTime + cfg.killDelay / 1000.0f > now)
        return;

    if (config->legitbotOnKey && !keyPressed)
        return;

    if (cfg.enabled && (cmd->buttons & UserCmd::IN_ATTACK || cfg.autoShot || cfg.aimlock)) {
        std::array<bool, 19> hitbox{ false };
        for (int i = 0; i < ARRAYSIZE(cfg.hitGroups); i++)
        {
            switch (i)
            {
            case 0: //Head
                hitbox[Hitbox::Head] = cfg.hitGroups[i];
                break;
            case 1: //Chest
                hitbox[Hitbox::Thorax] = cfg.hitGroups[i];
                hitbox[Hitbox::LowerChest] = cfg.hitGroups[i];
                hitbox[Hitbox::UpperChest] = cfg.hitGroups[i];
                break;
            case 2: //Stomach
                hitbox[Hitbox::Pelvis] = cfg.hitGroups[i];
                hitbox[Hitbox::Belly] = cfg.hitGroups[i];
                break;
            case 3: //Arms
                hitbox[Hitbox::RightUpperArm] = cfg.hitGroups[i];
                hitbox[Hitbox::RightForearm] = cfg.hitGroups[i];
                hitbox[Hitbox::LeftUpperArm] = cfg.hitGroups[i];
                hitbox[Hitbox::LeftForearm] = cfg.hitGroups[i];
                break;
            case 4: //Legs
                hitbox[Hitbox::RightCalf] = cfg.hitGroups[i];
                hitbox[Hitbox::RightThigh] = cfg.hitGroups[i];
                hitbox[Hitbox::LeftCalf] = cfg.hitGroups[i];
                hitbox[Hitbox::LeftThigh] = cfg.hitGroups[i];
                break;
            default:
                break;
            }
        }

        auto bestFov = cfg.fov;
        Vector bestTarget{ };
        Vector bestAngle{ };
        const auto localPlayerEyePosition = localPlayer->getEyePosition();

        const auto aimPunch = localPlayer->getAimPunch();

        for (int i = 1; i <= interfaces->engine->getMaxClients(); i++) {
            auto entity = interfaces->entityList->getEntity(i);
            if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive()
                || !entity->isOtherEnemy(localPlayer.get()) && !cfg.friendlyFire || entity->gunGameImmunity())
                continue;

            const Model* mod = entity->getModel();
            if (!mod)
                continue;

            StudioHdr* hdr = interfaces->modelInfo->getStudioModel(mod);
            matrix3x4 boneMatrices[256];
            entity->setupBones(boneMatrices, 256, 0x7FF00, memory->globalVars->currenttime);
            Animations::finishSetup(entity);

            for (int j = 0; j < 19; j++)
            {
                if (!(hitbox[j]))
                    continue;

                for (auto bonePosition : Aimbot::multiPoint(entity, boneMatrices, hdr, j, weaponClass, cfg.multiPoint)) {
                    const auto angle = Aimbot::calculateRelativeAngle(localPlayerEyePosition, bonePosition, cmd->viewangles + aimPunch);
                    const auto fov = std::hypot(angle.x, angle.y);

                    if (fov > bestFov)
                        continue;

                    if (!cfg.ignoreSmoke && memory->lineGoesThroughSmoke(localPlayerEyePosition, bonePosition, 1))
                        continue;

                    if (!localPlayer->isVisible(bonePosition) && cfg.visibleOnly)
                        continue;

                    auto damage = Aimbot::canScan(entity, bonePosition, activeWeapon->getWeaponData(), cfg.friendlyFire);
                    if (damage < cfg.minDamage)
                        continue;

                    if (cfg.scopedOnly && activeWeapon->isSniperRifle() && !localPlayer->isScoped() && localPlayer->flags() & 1 && !(cmd->buttons & (UserCmd::IN_JUMP))) {
                        if (cfg.autoScope)
                            cmd->buttons |= UserCmd::IN_ATTACK2;
                        return;
                    }

                    if (localPlayer->flags() & 1 && !(cmd->buttons & (UserCmd::IN_JUMP)))
                        shouldRunAutoStop.at(weaponClass) = cfg.autoStop;

                    cmd->tickCount = Backtrack::timeToTicks(entity->simulationTime() + Backtrack::getLerp());

                    if (!Aimbot::hitChance(localPlayer.get(), entity, activeWeapon, angle, cmd, cfg.hitChance))
                        continue;

                    if (fov < bestFov) {
                        bestFov = fov;
                        bestTarget = bonePosition;
                        bestAngle = angle;
                    }
                }

                const auto records = Backtrack::getRecords(i);
                if (!records || records->empty() || records->size() <= 3 || !Backtrack::valid(records->front().simulationTime))
                    continue;

                int bestRecord{ };
                auto recordFov = 255.f;

                for (size_t p = 0; p < records->size(); p++) {
                    const auto& record = records->at(p);
                    if (!Backtrack::valid(record.simulationTime))
                        continue;

                    const auto angle = Aimbot::calculateRelativeAngle(localPlayerEyePosition, record.head, cmd->viewangles + aimPunch);
                    const auto fov = std::hypotf(angle.x, angle.y);

                    if (fov < recordFov) {
                        recordFov = fov;
                        bestRecord = p;
                    }
                }

                if (!bestRecord)
                    continue;

                auto currentRecord = records->at(bestRecord);
                Animations::setup(entity, currentRecord);
                for (auto bonePosition : Aimbot::multiPoint(entity, currentRecord.matrix, currentRecord.hdr, j, weaponClass, cfg.multiPoint))
                {
                    const auto angle = Aimbot::calculateRelativeAngle(localPlayerEyePosition, bonePosition, cmd->viewangles + aimPunch);
                    const auto fov = std::hypot(angle.x, angle.y);

                    if (fov > bestFov)
                        continue;

                    if (!cfg.ignoreSmoke && memory->lineGoesThroughSmoke(localPlayerEyePosition, bonePosition, 1))
                        continue;

                    if (!localPlayer->isVisible(bonePosition) && cfg.visibleOnly)
                        continue;

                    auto damage = Aimbot::canScan(entity, bonePosition, activeWeapon->getWeaponData(), cfg.friendlyFire);
                    if (damage < cfg.minDamage)
                        continue;

                    if (cfg.scopedOnly && activeWeapon->isSniperRifle() && !localPlayer->isScoped() && localPlayer->flags() & 1 && !(cmd->buttons & (UserCmd::IN_JUMP))) {
                        if (cfg.autoScope)
                            cmd->buttons |= UserCmd::IN_ATTACK2;
                        return;
                    }

                    if (localPlayer->flags() & 1 && !(cmd->buttons & (UserCmd::IN_JUMP)))
                        shouldRunAutoStop.at(weaponClass) = cfg.autoStop;

                    cmd->tickCount = Backtrack::timeToTicks(currentRecord.simulationTime + Backtrack::getLerp());
                    Animations::setup(entity, currentRecord);

                    if (!Aimbot::hitChance(localPlayer.get(), entity, activeWeapon, angle, cmd, cfg.hitChance))
                        continue;

                    if (fov < bestFov) {
                        bestFov = fov;
                        bestTarget = bonePosition;
                        bestAngle = angle;
                    }
                }
                Animations::finishSetup(entity);
            }
        }

        if (bestTarget.notNull()) {
            static Vector lastAngles{ cmd->viewangles };
            static int lastCommand{ };

            if (lastCommand == cmd->commandNumber - 1 && lastAngles.notNull() && cfg.silent)
                cmd->viewangles = lastAngles;

            auto angle = Aimbot::calculateRelativeAngle(localPlayerEyePosition, bestTarget, cmd->viewangles + aimPunch);
            bool clamped{ false };

            float maxAngleDelta{ Misc::shouldAimStep() ? 39.f : 255.f };
            if (std::abs(angle.x) > maxAngleDelta || std::abs(angle.y) > maxAngleDelta) {
                angle.x = std::clamp(angle.x, -maxAngleDelta, maxAngleDelta);
                angle.y = std::clamp(angle.y, -maxAngleDelta, maxAngleDelta);
                clamped = true;
            }

            angle /= cfg.smooth;
            cmd->viewangles += angle;
            if (!cfg.silent)
                interfaces->engine->setViewAngles(cmd->viewangles);

            if (cfg.autoShot && activeWeapon->nextPrimaryAttack() <= memory->globalVars->serverTime() && !clamped)
                cmd->buttons |= UserCmd::IN_ATTACK;

            if (clamped)
                cmd->buttons &= ~UserCmd::IN_ATTACK;

            if (clamped || cfg.smooth > 1.0f) lastAngles = cmd->viewangles;
            else lastAngles = Vector{ };

            lastCommand = cmd->commandNumber;
        }
    }
}
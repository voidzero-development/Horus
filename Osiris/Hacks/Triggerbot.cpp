#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"
#include "../SDK/Entity.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/UserCmd.h"
#include "../SDK/WeaponData.h"
#include "../SDK/WeaponId.h"
#include "../SDK/ModelInfo.h"
#include "Aimbot.h"
#include "Animations.h"
#include "Backtrack.h"
#include "Triggerbot.h"

static bool keyPressed;

void Triggerbot::run(UserCmd* cmd) noexcept
{
    if (!localPlayer || !localPlayer->isAlive() || localPlayer->nextAttack() > memory->globalVars->serverTime() || localPlayer->isDefusing() || localPlayer->waitForNoAttack())
        return;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon || !activeWeapon->clip() || activeWeapon->nextPrimaryAttack() > memory->globalVars->serverTime())
        return;

    if (localPlayer->shotsFired() > 0 && !activeWeapon->isFullAuto())
        return;

    auto weaponIndex = getWeaponIndex(activeWeapon->itemDefinitionIndex2());
    if (!weaponIndex)
        return;

    auto weaponClass = getWeaponClass(activeWeapon->itemDefinitionIndex2());
    if (!config->triggerbot[weaponClass].enabled)
        weaponClass = 0;

    const auto& cfg = config->triggerbot[weaponClass];

    if (!cfg.enabled)
        return;

    static auto lastTime = 0.0f;
    static auto lastContact = 0.0f;

    const auto now = memory->globalVars->realtime;

    if (now - lastContact < cfg.burstTime) {
        cmd->buttons |= UserCmd::IN_ATTACK;
        return;
    }
    lastContact = 0.0f;

    if (!keyPressed)
        return;

    if (now - lastTime < cfg.shotDelay / 1000.0f)
        return;

    if (!cfg.ignoreFlash && localPlayer->isFlashed())
        return;

    if (cfg.scopedOnly && activeWeapon->isSniperRifle() && !localPlayer->isScoped())
        return;

    const auto weaponData = activeWeapon->getWeaponData();
    if (!weaponData)
        return;

    const auto startPos = localPlayer->getEyePosition();
    const auto endPos = startPos + Vector::fromAngle(cmd->viewangles + localPlayer->getAimPunch()) * weaponData->range;

    if (!cfg.ignoreSmoke && memory->lineGoesThroughSmoke(startPos, endPos, 1))
        return;

    Trace trace;
    interfaces->engineTrace->traceRay({ startPos, endPos }, 0x46004009, localPlayer.get(), trace);

    lastTime = now;

    if ((!trace.entity || !trace.entity->isPlayer()))
    {
        auto bestFov{ 255.f };
        Vector bestTargetHead{ };
        int bestRecord = -1;
        int bestTargetIndex{ };
        Entity* bestTarget{ };

        for (int i = 1; i <= interfaces->engine->getMaxClients(); i++) {
            auto entity = interfaces->entityList->getEntity(i);
            if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive()
                || !entity->isOtherEnemy(localPlayer.get()))
                continue;

            const auto records = Backtrack::getRecords(i);

            Animations::finishSetup(entity);

            auto head = entity->getBonePosition(8);

            auto angle = Aimbot::calculateRelativeAngle(localPlayer->getEyePosition(), head, cmd->viewangles + localPlayer->getAimPunch());
            auto fov = std::hypotf(angle.x, angle.y);
            if (fov < bestFov) {
                bestFov = fov;
                bestTarget = entity;
                bestTargetIndex = i;
                bestTargetHead = head;
            }
        }

        if (bestTarget) {
            const auto records = Backtrack::getRecords(bestTargetIndex);
            if (!records || records->empty() || records->size() <= 3 || !Backtrack::valid(records->front().simulationTime) || (!cfg.ignoreSmoke && memory->lineGoesThroughSmoke(localPlayer->getEyePosition(), bestTargetHead, 1)))
                return;

            bestFov = 255.f;

            for (size_t p = 0; p < records->size(); p++) {
                const auto& record = records->at(p);
                if (!Backtrack::valid(record.simulationTime))
                    continue;

                const auto angle = Aimbot::calculateRelativeAngle(localPlayer->getEyePosition(), record.head, cmd->viewangles + localPlayer->getAimPunch());
                const auto fov = std::hypotf(angle.x, angle.y);

                if (fov < bestFov) {
                    bestFov = fov;
                    bestRecord = p;
                }
            }
        }

        if (bestRecord != -1) {
            const auto& record = Backtrack::getRecords(bestTargetIndex)->at(bestRecord);
            Animations::setup(bestTarget, record);
            cmd->tickCount = Backtrack::timeToTicks(record.simulationTime + Backtrack::getLerp());
            interfaces->engineTrace->traceRay({ startPos, endPos }, 0x46004009, localPlayer.get(), trace);
        }
    }

    if (!trace.entity || !trace.entity->isPlayer())
        return;

    if (!cfg.friendlyFire && !localPlayer->isOtherEnemy(trace.entity))
        return;

    if (trace.entity->gunGameImmunity())
        return;

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
    for (int j = 0; j < 19; j++)
    {
        if (trace.hitbox == j && !hitbox[j])
            return;
    }

    const auto aimPunch = localPlayer->getAimPunch();
    const auto angle = Aimbot::calculateRelativeAngle(localPlayer->getEyePosition(), trace.endpos, cmd->viewangles + aimPunch);

    float damage = (activeWeapon->itemDefinitionIndex2() != WeaponId::Taser ? HitGroup::getDamageMultiplier(trace.hitgroup) : 1.0f) * weaponData->damage * std::pow(weaponData->rangeModifier, trace.fraction * weaponData->range / 500.0f);
    bool hitChance = Aimbot::hitChance(localPlayer.get(), trace.entity, activeWeapon, angle, cmd, cfg.hitChance);

    if (!hitChance)
        return;

    if (float armorRatio{ weaponData->armorRatio / 2.0f }; activeWeapon->itemDefinitionIndex2() != WeaponId::Taser && HitGroup::isArmored(trace.hitgroup, trace.entity->hasHelmet()))
        damage -= (trace.entity->armor() < damage * armorRatio / 2.0f ? trace.entity->armor() * 4.0f : damage) * (1.0f - armorRatio);

    if (damage >= (cfg.killshot ? trace.entity->health() : cfg.minDamage)) {
        cmd->buttons |= UserCmd::IN_ATTACK;
        lastTime = 0.0f;
        lastContact = now;
    }
}

void Triggerbot::updateInput() noexcept
{
    keyPressed = config->triggerbotHoldKey == KeyBind::NONE || config->triggerbotHoldKey.isDown();
}
#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"
#include "../SDK/Entity.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/UserCmd.h"
#include "../SDK/WeaponData.h"
#include "../SDK/WeaponId.h"
#include "../SDK/ModelInfo.h"
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

    auto weaponClass = getWeaponClass(activeWeapon->itemDefinitionIndex2());
    if (!config->aimbot[weaponClass].enabled)
        weaponClass = 0;

    const auto& cfg = config->triggerbot[weaponClass];

    if (!cfg.enabled)
        return;

    static auto lastTime = 0.0f;
    static auto lastContact = 0.0f;

    const auto now = memory->globalVars->realtime;

    if (now - lastContact < config->triggerbot[weaponClass].burstTime) {
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

    if (!trace.entity || !trace.entity->isPlayer())
        return;

    if (!cfg.friendlyFire && !localPlayer->isOtherEnemy(trace.entity))
        return;

    if (trace.entity->gunGameImmunity())
        return;

    std::array<bool, 19> hitbox{ false };
    for (int i = 0; i < ARRAYSIZE(config->triggerbot[weaponClass].hitGroups); i++)
    {
        switch (i)
        {
        case 0: //Head
            hitbox[Hitbox::Head] = config->triggerbot[weaponClass].hitGroups[i];
            break;
        case 1: //Chest
            hitbox[Hitbox::Thorax] = config->triggerbot[weaponClass].hitGroups[i];
            hitbox[Hitbox::LowerChest] = config->triggerbot[weaponClass].hitGroups[i];
            hitbox[Hitbox::UpperChest] = config->triggerbot[weaponClass].hitGroups[i];
            break;
        case 2: //Stomach
            hitbox[Hitbox::Pelvis] = config->triggerbot[weaponClass].hitGroups[i];
            hitbox[Hitbox::Belly] = config->triggerbot[weaponClass].hitGroups[i];
            break;
        case 3: //Arms
            hitbox[Hitbox::RightUpperArm] = config->triggerbot[weaponClass].hitGroups[i];
            hitbox[Hitbox::RightForearm] = config->triggerbot[weaponClass].hitGroups[i];
            hitbox[Hitbox::LeftUpperArm] = config->triggerbot[weaponClass].hitGroups[i];
            hitbox[Hitbox::LeftForearm] = config->triggerbot[weaponClass].hitGroups[i];
            break;
        case 4: //Legs
            hitbox[Hitbox::RightCalf] = config->triggerbot[weaponClass].hitGroups[i];
            hitbox[Hitbox::RightThigh] = config->triggerbot[weaponClass].hitGroups[i];
            hitbox[Hitbox::LeftCalf] = config->triggerbot[weaponClass].hitGroups[i];
            hitbox[Hitbox::LeftThigh] = config->triggerbot[weaponClass].hitGroups[i];
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

    float damage = (activeWeapon->itemDefinitionIndex2() != WeaponId::Taser ? HitGroup::getDamageMultiplier(trace.hitgroup) : 1.0f) * weaponData->damage * std::pow(weaponData->rangeModifier, trace.fraction * weaponData->range / 500.0f);

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

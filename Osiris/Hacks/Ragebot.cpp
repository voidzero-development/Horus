#include "Aimbot.h"
#include "Animations.h"
#include "Backtrack.h"
#include "Ragebot.h"
#include "Misc.h"
#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"
#include "../SDK/Angle.h"
#include "../SDK/Entity.h"
#include "../SDK/UserCmd.h"
#include "../SDK/Vector.h"
#include "../SDK/WeaponId.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/PhysicsSurfaceProps.h"
#include "../SDK/WeaponData.h"
#include "../SDK/StudioRender.h"
#include "../SDK/ModelInfo.h"

static bool keyPressed = false;

void Ragebot::updateInput() noexcept
{
    if (config->ragebotKeyMode == 0)
        keyPressed = config->ragebotKey.isDown();
    if (config->ragebotKeyMode == 1 && config->ragebotKey.isPressed())
        keyPressed = !keyPressed;
}

static bool traceToExit(const Trace& enterTrace, const Vector& start, const Vector& direction, Vector& end, Trace& exitTrace)
{
    bool result = false;
#ifdef _WIN32
    const auto traceToExitFn = memory->traceToExit;
    __asm {
        push exitTrace
        mov eax, direction
        push[eax]Vector.z
        push[eax]Vector.y
        push[eax]Vector.x
        mov eax, start
        push[eax]Vector.z
        push[eax]Vector.y
        push[eax]Vector.x
        mov edx, enterTrace
        mov ecx, end
        call traceToExitFn
        add esp, 28
        mov result, al
    }
#endif
    return result;
}

static float handleBulletPenetration(SurfaceData* enterSurfaceData, const Trace& enterTrace, const Vector& direction, Vector& result, float penetration, float damage) noexcept
{
    Vector end;
    Trace exitTrace;

    if (!traceToExit(enterTrace, enterTrace.endpos, direction, end, exitTrace))
        return -1.0f;

    SurfaceData* exitSurfaceData = interfaces->physicsSurfaceProps->getSurfaceData(exitTrace.surface.surfaceProps);

    float damageModifier = 0.16f;
    float penetrationModifier = (enterSurfaceData->penetrationmodifier + exitSurfaceData->penetrationmodifier) / 2.0f;

    if (enterSurfaceData->material == 71 || enterSurfaceData->material == 89) {
        damageModifier = 0.05f;
        penetrationModifier = 3.0f;
    }
    else if (enterTrace.contents >> 3 & 1 || enterTrace.surface.flags >> 7 & 1) {
        penetrationModifier = 1.0f;
    }

    if (enterSurfaceData->material == exitSurfaceData->material) {
        if (exitSurfaceData->material == 85 || exitSurfaceData->material == 87)
            penetrationModifier = 3.0f;
        else if (exitSurfaceData->material == 76)
            penetrationModifier = 2.0f;
    }

    damage -= 11.25f / penetration / penetrationModifier + damage * damageModifier + (exitTrace.endpos - enterTrace.endpos).squareLength() / 24.0f / penetrationModifier;

    result = exitTrace.endpos;
    return damage;
}

static float canScan(Entity* entity, const Vector& destination, const WeaponInfo* weaponData, bool allowFriendlyFire) noexcept
{
    if (!localPlayer)
        return 0;

    float damage{ static_cast<float>(weaponData->damage) };

    Vector start{ localPlayer->getEyePosition() };
    Vector direction{ destination - start };
    direction /= direction.length();

    int hitsLeft = 4;

    while (damage >= 1.0f && hitsLeft) {
        Trace trace;
        interfaces->engineTrace->traceRay({ start, destination }, 0x4600400B, localPlayer.get(), trace);

        if (!allowFriendlyFire && trace.entity && trace.entity->isPlayer() && !localPlayer->isOtherEnemy(trace.entity))
            return false;

        if (trace.entity && trace.entity->isPlayer() && trace.entity == entity && trace.hitgroup > HitGroup::Generic && trace.hitgroup <= HitGroup::RightLeg) {
            damage = HitGroup::getDamageMultiplier(trace.hitgroup) * damage * powf(weaponData->rangeModifier, trace.fraction * weaponData->range / 500.0f);

            if (float armorRatio{ weaponData->armorRatio / 2.0f }; HitGroup::isArmored(trace.hitgroup, trace.entity->hasHelmet()))
                damage -= (trace.entity->armor() < damage * armorRatio / 2.0f ? trace.entity->armor() * 4.0f : damage) * (1.0f - armorRatio);

            return damage;
        }

        if (trace.fraction == 1.0f)
            break;

        const auto surfaceData = interfaces->physicsSurfaceProps->getSurfaceData(trace.surface.surfaceProps);

        if (surfaceData->penetrationmodifier < 0.1f)
            break;

        damage = handleBulletPenetration(surfaceData, trace, direction, start, weaponData->penetration, damage);
        hitsLeft--;
    }
    return 0;
}

static std::vector<Vector> multiPoint(Entity* entity, matrix3x4 matrix[256], StudioHdr* hdr, int iHitbox, int weaponClass, int multiPoint) noexcept
{
    auto angleVectors = [](const Vector& angles, Vector* forward)
    {
        float	sp, sy, cp, cy;

        sy = sin(degreesToRadians(angles.y));
        cy = cos(degreesToRadians(angles.y));

        sp = sin(degreesToRadians(angles.x));
        cp = cos(degreesToRadians(angles.x));

        forward->x = cp * cy;
        forward->y = cp * sy;
        forward->z = -sp;
    };

    auto vectorTransformWrapper = [](const Vector& in1, const matrix3x4 in2, Vector& out)
    {
        auto vectorTransform = [](const float* in1, const matrix3x4 in2, float* out)
        {
            auto dotProducts = [](const float* v1, const float* v2)
            {
                return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
            };
            out[0] = dotProducts(in1, in2[0]) + in2[0][3];
            out[1] = dotProducts(in1, in2[1]) + in2[1][3];
            out[2] = dotProducts(in1, in2[2]) + in2[2][3];
        };
        vectorTransform(&in1.x, in2, &out.x);
    };

    if (!hdr)
        return {};
    StudioHitboxSet* set = hdr->getHitboxSet(0);
    if (!set)
        return {};
    StudioBbox* hitbox = set->getHitbox(iHitbox);
    if (!hitbox)
        return {};
    Vector vMin, vMax, vCenter;
    vectorTransformWrapper(hitbox->bbMin, matrix[hitbox->bone], vMin);
    vectorTransformWrapper(hitbox->bbMax, matrix[hitbox->bone], vMax);
    vCenter = (vMin + vMax) * 0.5f;

    std::vector<Vector> vecArray;

    if (multiPoint == 0)
    {
        vecArray.emplace_back(vCenter);
        return vecArray;
    }

    Vector currentAngles = Aimbot::calculateRelativeAngle(vCenter, localPlayer->getEyePosition(), Vector{});

    Vector forward;
    angleVectors(currentAngles, &forward);

    Vector right = forward.cross(Vector{ 0, 0, 1 });
    Vector left = Vector{ -right.x, -right.y, right.z };

    Vector top = Vector{ 0, 0, 1 };
    Vector bot = Vector{ 0, 0, -1 };

    float amount = (min(multiPoint, 95)) * 0.01f;

    switch (iHitbox) {
    case Hitbox::Head:
        for (auto i = 0; i < 4; ++i)
            vecArray.emplace_back(vCenter);

        vecArray[1] += top * (hitbox->capsuleRadius * amount);
        vecArray[2] += right * (hitbox->capsuleRadius * amount);
        vecArray[3] += left * (hitbox->capsuleRadius * amount);
        break;
    case Hitbox::RightUpperArm:
    case Hitbox::RightForearm:
    case Hitbox::LeftUpperArm:
    case Hitbox::RightCalf:
    case Hitbox::RightThigh:
    case Hitbox::LeftCalf:
    case Hitbox::LeftThigh:
        break;
    default:
        for (auto i = 0; i < 3; ++i)
            vecArray.emplace_back(vCenter);

        vecArray[1] += right * (hitbox->capsuleRadius * amount);
        vecArray[2] += left * (hitbox->capsuleRadius * amount);
        break;
    }
    return vecArray;
}

static bool hitChance(Entity* localPlayer, Entity* entity, Entity* weaponData, const Vector& destination, const UserCmd* cmd, const int hitChance) noexcept
{
    if (!hitChance)
        return true;

    constexpr int maxSeed = 256;

    const Angle angles(destination + cmd->viewangles);

    int hits = 0;
    const int hitsNeed = static_cast<int>(static_cast<float>(maxSeed) * (static_cast<float>(hitChance) / 100.f));

    const auto weapSpread = weaponData->getSpread();
    const auto weapInaccuracy = weaponData->getInaccuracy();
    const auto localEyePosition = localPlayer->getEyePosition();
    const auto range = weaponData->getWeaponData()->range;

    for (int i = 0; i < maxSeed; ++i)
    {
        memory->randomSeed(i + 1);
        float inaccuracy = memory->randomFloat(0.f, 1.f);
        float spread = memory->randomFloat(0.f, 1.f);
        const float spreadX = memory->randomFloat(0.f, 2.f * static_cast<float>(M_PI));
        const float spreadY = memory->randomFloat(0.f, 2.f * static_cast<float>(M_PI));

        const auto weaponIndex = weaponData->itemDefinitionIndex2();
        const auto recoilIndex = weaponData->recoilIndex();
        if (weaponIndex == WeaponId::Revolver)
        {
            if (cmd->buttons & UserCmd::IN_ATTACK2)
            {
                inaccuracy = 1.f - inaccuracy * inaccuracy;
                spread = 1.f - spread * spread;
            }
        }
        else if (weaponIndex == WeaponId::Negev && recoilIndex < 3.f)
        {
            for (int i = 3; i > recoilIndex; --i)
            {
                inaccuracy *= inaccuracy;
                spread *= spread;
            }

            inaccuracy = 1.f - inaccuracy;
            spread = 1.f - spread;
        }

        inaccuracy *= weapInaccuracy;
        spread *= weapSpread;

        Vector spreadView{ (cosf(spreadX) * inaccuracy) + (cosf(spreadY) * spread),
                           (sinf(spreadX) * inaccuracy) + (sinf(spreadY) * spread) };
        Vector direction{ (angles.forward + (angles.right * spreadView.x) + (angles.up * spreadView.y)) * range };

        static Trace trace;
        interfaces->engineTrace->clipRayToEntity({ localEyePosition, localEyePosition + direction }, 0x4600400B, entity, trace);
        if (trace.entity == entity)
            ++hits;

        if (hits >= hitsNeed)
            return true;

        if ((maxSeed - i + hits) < hitsNeed)
            return false;
    }
    return false;
}

static std::array<bool, 7> shouldRunAutoStop;

void Ragebot::autoStop(UserCmd* cmd) noexcept
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
    if (!config->ragebot[weaponClass].enabled)
        weaponClass = 0;

    const auto& cfg = config->ragebot[weaponClass];

    if (!cfg.autoStop || !shouldRunAutoStop.at(weaponClass))
        return;

    if (activeWeapon->nextPrimaryAttack() > memory->globalVars->serverTime())
        return;

    if (!cfg.ignoreFlash && localPlayer->isFlashed())
        return;

    if (config->ragebotOnKey && !keyPressed)
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

void Ragebot::run(UserCmd* cmd) noexcept
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
    if (!config->ragebot[weaponClass].enabled)
        weaponClass = 0;

    const auto& cfg = config->ragebot[weaponClass];

    if (activeWeapon->nextPrimaryAttack() > memory->globalVars->serverTime())
        return;

    if (!cfg.ignoreFlash && localPlayer->isFlashed())
        return;

    if (config->ragebotOnKey && !keyPressed)
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
        auto bestDamage = config->ragebot[weaponClass].minDamage;
        Vector bestTarget{ };
        Vector bestAngle{ };
        const auto localPlayerEyePosition = localPlayer->getEyePosition();
        const auto localPlayerOrigin = localPlayer->getAbsOrigin();

        const auto aimPunch = localPlayer->getAimPunch();

        std::vector<Ragebot::Enemies> enemies;

        for (int i = 1; i <= interfaces->engine->getMaxClients(); i++) {
            const auto entity{ interfaces->entityList->getEntity(i) };
            if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive()
                || !entity->isOtherEnemy(localPlayer.get()) && !cfg.friendlyFire || entity->gunGameImmunity())
                continue;

            const auto angle = Aimbot::calculateRelativeAngle(localPlayerEyePosition, entity->getBonePosition(8), cmd->viewangles + aimPunch);
            const auto& origin = entity->getAbsOrigin();
            const auto fov = std::hypot(angle.x, angle.y); //Fov
            const auto health = entity->health(); //Health
            const auto distance = (origin - localPlayer->getAbsOrigin()).length(); //Distance
            enemies.emplace_back(i, health, distance, fov);
        }

        switch (cfg.priority)
        {
        case 0:
            std::sort(enemies.begin(), enemies.end(), healthSort);
            break;
        case 1:
            std::sort(enemies.begin(), enemies.end(), distanceSort);
            break;
        case 2:
            std::sort(enemies.begin(), enemies.end(), fovSort);
            break;
        default:
            break;
        }

        for (const auto& target : enemies) {
            const auto entity{ interfaces->entityList->getEntity(target.id) };
            Animations::finishSetup(entity);

            const Model* mod = entity->getModel();
            if (!mod)
                continue;

            StudioHdr* hdr = interfaces->modelInfo->getStudioModel(mod);
            matrix3x4 boneMatrices[256];
            entity->setupBones(boneMatrices, 256, 0x7FF00, memory->globalVars->currenttime);

            for (int j = 0; j < 19; j++)
            {
                if (!(hitbox[j]))
                    continue;

                for (auto bonePosition : multiPoint(entity, boneMatrices, hdr, j, weaponClass, cfg.multiPoint)) {
                    const auto angle = Aimbot::calculateRelativeAngle(localPlayerEyePosition, bonePosition, cmd->viewangles + aimPunch);
                    const auto fov = std::hypot(angle.x, angle.y);

                    if (fov > bestFov)
                        continue;

                    if (!localPlayer->isVisible(bonePosition) && cfg.visibleOnly)
                        continue;

                    auto damage = canScan(entity, bonePosition, activeWeapon->getWeaponData(), cfg.friendlyFire);
                    if (damage <= 0.f)
                        continue;

                    if (!cfg.ignoreSmoke && memory->lineGoesThroughSmoke(localPlayerEyePosition, bonePosition, 1))
                        continue;

                    if (cfg.scopedOnly && activeWeapon->isSniperRifle() && !localPlayer->isScoped() && localPlayer->flags() & 1 && !(cmd->buttons & (UserCmd::IN_JUMP))) {
                        if (cfg.autoScope)
                            cmd->buttons |= UserCmd::IN_ATTACK2;
                        return;
                    }

                    if (localPlayer->flags() & 1 && !(cmd->buttons & (UserCmd::IN_JUMP)))
                        shouldRunAutoStop.at(weaponClass) = cfg.autoStop;

                    cmd->tickCount = Backtrack::timeToTicks(entity->simulationTime() + Backtrack::getLerp());

                    if (!hitChance(localPlayer.get(), entity, activeWeapon, angle, cmd, cfg.hitChance))
                        continue;

                    if (bestDamage <= damage) {
                        bestDamage = damage;
                        bestTarget = bonePosition;
                        bestAngle = angle;
                    }
                }

                const auto records = Backtrack::getRecords(target.id);
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
                for (auto bonePosition : multiPoint(entity, currentRecord.matrix, currentRecord.hdr, j, weaponClass, cfg.multiPoint))
                {
                    const auto angle = Aimbot::calculateRelativeAngle(localPlayerEyePosition, bonePosition, cmd->viewangles + aimPunch);
                    const auto fov = std::hypot(angle.x, angle.y);

                    if (fov > bestFov)
                        continue;

                    if (!localPlayer->isVisible(bonePosition) && cfg.visibleOnly)
                        continue;

                    auto damage = canScan(entity, bonePosition, activeWeapon->getWeaponData(), cfg.friendlyFire);
                    if (damage <= 0.f)
                        continue;

                    if (!cfg.ignoreSmoke && memory->lineGoesThroughSmoke(localPlayerEyePosition, bonePosition, 1))
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

                    if (!hitChance(localPlayer.get(), entity, activeWeapon, angle, cmd, cfg.hitChance))
                        continue;

                    if (bestDamage <= damage) {
                        bestDamage = damage;
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

            cmd->viewangles += angle;
            if (!cfg.silent)
                interfaces->engine->setViewAngles(cmd->viewangles);

            if (cfg.autoShot && activeWeapon->nextPrimaryAttack() <= memory->globalVars->serverTime() && !clamped)
                cmd->buttons |= UserCmd::IN_ATTACK;

            if (clamped)
                cmd->buttons &= ~UserCmd::IN_ATTACK;

            if (clamped) lastAngles = cmd->viewangles;
            else lastAngles = Vector{ };

            lastCommand = cmd->commandNumber;
        }
    }
}
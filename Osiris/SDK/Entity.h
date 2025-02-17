#pragma once

#include "AnimState.h"
#include "ClientClass.h"
#include "Cvar.h"
#include "Engine.h"
#include "EngineTrace.h"
#include "EntityList.h"
#include "GlobalVars.h"
#include "LocalPlayer.h"
#include "matrix3x4.h"
#include "ModelRender.h"
#include "Utils.h"
#include "VarMapping.h"
#include "Vector.h"
#include "VirtualMethod.h"
#include "WeaponData.h"
#include "WeaponId.h"

#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"
#include "../Netvars.h"

#include <functional>

struct AnimState;

struct AnimationLayer
{
public:
    bool clientblend;
    float blendin;
    void* studioHdr;
    int dispatchedsrc;
    int dispatcheddst;
    unsigned int order;
    unsigned int sequence;
    float prevcycle;
    float weight;
    float weightdeltarate;
    float playbackRate;
    float cycle;
    void* owner;
    int invalidatephysicsbits;
};

enum class MoveType {
    NOCLIP = 8,
    LADDER = 9
};

enum class ObsMode {
    None = 0,
    Deathcam,
    Freezecam,
    Fixed,
    InEye,
    Chase,
    Roaming
};

enum class Team {
    None = 0,
    Spectators,
    TT,
    CT
};

class Collideable {
public:
    VIRTUAL_METHOD(Vector&, obbMins, 1, (), (this))
    VIRTUAL_METHOD(Vector&, obbMaxs, 2, (), (this))
};

class Entity {
public:
    VIRTUAL_METHOD(void, release, 1, (), (this + sizeof(uintptr_t) * 2))
    VIRTUAL_METHOD(ClientClass*, getClientClass, 2, (), (this + sizeof(uintptr_t) * 2))
    VIRTUAL_METHOD(void, preDataUpdate, 6, (int updateType), (this + sizeof(uintptr_t) * 2, updateType))
    VIRTUAL_METHOD(void, postDataUpdate, 7, (int updateType), (this + sizeof(uintptr_t) * 2, updateType))
    VIRTUAL_METHOD(bool, isDormant, 9, (), (this + sizeof(uintptr_t) * 2))
    VIRTUAL_METHOD(int, index, 10, (), (this + sizeof(uintptr_t) * 2))
    VIRTUAL_METHOD(void, setDestroyedOnRecreateEntities, 13, (), (this + sizeof(uintptr_t) * 2))

    VIRTUAL_METHOD(bool, shouldDraw, WIN32_LINUX(3, 149), (), (this + WIN32_LINUX(sizeof(uintptr_t), 0)))
    VIRTUAL_METHOD(Vector&, getRenderOrigin, 1, (), (this + 4))
    VIRTUAL_METHOD(const Model*, getModel, 8, (), (this + sizeof(uintptr_t)))
    VIRTUAL_METHOD(const matrix3x4&, toWorldTransform, 32, (), (this + sizeof(uintptr_t)))

    VIRTUAL_METHOD_V(int&, handle, 2, (), (this))
    VIRTUAL_METHOD_V(Collideable*, getCollideable, 3, (), (this))

    VIRTUAL_METHOD(const Vector&, getAbsOrigin, WIN32_LINUX(10, 12), (), (this))
    VIRTUAL_METHOD(Vector&, getAbsAngle, 11, (), (this))
    VIRTUAL_METHOD(void, setModelIndex, WIN32_LINUX(75, 111), (int index), (this, index))
    VIRTUAL_METHOD(bool, getAttachment, WIN32_LINUX(83, 121), (int index, Vector& origin), (this, index, std::ref(origin)))
    VIRTUAL_METHOD(Team, getTeamNumber, WIN32_LINUX(87, 127), (), (this))
    VIRTUAL_METHOD(int, health, WIN32_LINUX(121, 166), (), (this))
    VIRTUAL_METHOD(bool, isAlive, WIN32_LINUX(155, 207), (), (this))
    VIRTUAL_METHOD(bool, isPlayer, WIN32_LINUX(157, 209), (), (this))
    VIRTUAL_METHOD(bool, isWeapon, WIN32_LINUX(165, 217), (), (this))
    VIRTUAL_METHOD(Entity*, getActiveWeapon, WIN32_LINUX(267, 330), (), (this))
    VIRTUAL_METHOD(int, getWeaponSubType, WIN32_LINUX(281, 349), (), (this))
    VIRTUAL_METHOD(ObsMode, getObserverMode, WIN32_LINUX(293, 356), (), (this))
    VIRTUAL_METHOD(Entity*, getObserverTarget, WIN32_LINUX(294, 357), (), (this))
    VIRTUAL_METHOD(WeaponType, getWeaponType, WIN32_LINUX(454, 522), (), (this))
    VIRTUAL_METHOD(WeaponInfo*, getWeaponData, WIN32_LINUX(460, 528), (), (this))
    VIRTUAL_METHOD(int, getMuzzleAttachmentIndex1stPerson, WIN32_LINUX(467, 535), (Entity* viewModel), (this, viewModel))
    VIRTUAL_METHOD(int, getMuzzleAttachmentIndex3rdPerson, WIN32_LINUX(468, 536), (), (this))
    VIRTUAL_METHOD(float, getInaccuracy, WIN32_LINUX(482, 550), (), (this))
    VIRTUAL_METHOD(float, getSpread, 452, (), (this))
    VIRTUAL_METHOD(void, UpdateClientSideAnimation, 223, (), (this))

#if IS_WIN32()
    auto getEyePosition() noexcept
    {
        Vector v;
        VirtualMethod::call<void, 284>(this, std::ref(v));
        return v;
    }

    auto getAimPunch() noexcept
    {
        Vector v;
        VirtualMethod::call<void, 345>(this, std::ref(v));
        return v;
    }
#else
    VIRTUAL_METHOD(Vector, getEyePosition, 347, (), (this))
    VIRTUAL_METHOD(Vector, getAimPunch, 408, (), (this))
#endif

    auto isPistol() noexcept { return getWeaponType() == WeaponType::Pistol; }
    auto isSniperRifle() noexcept { return getWeaponType() == WeaponType::SniperRifle; }
    auto isGrenade() noexcept { return getWeaponType() == WeaponType::Grenade; }
    auto isKnife() noexcept { return getWeaponType() == WeaponType::Knife; }

    auto isFullAuto() noexcept
    {
        const auto weaponData = getWeaponData();
        if (weaponData)
            return weaponData->fullAuto;
        return false;
    }

    auto requiresRecoilControl() noexcept
    {
        const auto weaponData = getWeaponData();
        if (weaponData)
            return weaponData->recoilMagnitude < 35.0f && weaponData->recoveryTimeStand > weaponData->cycletime;
        return false;
    }

    bool setupBones(matrix3x4* out, int maxBones, int boneMask, float currentTime) noexcept
    {
        if (localPlayer && this == localPlayer.get() && localPlayer->isAlive())
        {
            int* effects = reinterpret_cast<int*>(this + 0xF0);
            int* shouldSkipFrame = reinterpret_cast<int*>(this + 0xA68);
            int backupEffects = *effects;
            int backupShouldSkipFrame = *shouldSkipFrame;
            *shouldSkipFrame = 0;
            *effects |= 8;
            auto result = VirtualMethod::call<bool, 13>(this + 4, out, maxBones, boneMask, currentTime);
            *effects = backupEffects;
            *shouldSkipFrame = backupShouldSkipFrame;
            return result;
        }
        else
        {
            *reinterpret_cast<int*>(this + 0xA28) = 0;
            *reinterpret_cast<int*>(this + 0xA30) = memory->globalVars->framecount;
            int* render = reinterpret_cast<int*>(this + 0x274);
            int* shouldSkipFrame = (int*)(this + 0xA68);
            int* effects = reinterpret_cast<int*>(this + 0xF0);
            int backup = *render;
            int backupEffects = *effects;
            *shouldSkipFrame = 0;
            *render = 0;
            boneMask |= 0x200;
            *effects |= 8;
            auto result = VirtualMethod::call<bool, 13>(this + 4, out, maxBones, boneMask, currentTime);
            *render = backup;
            *effects = backupEffects;
            return result;
        }
        return VirtualMethod::call<bool, 13>(this + 4, out, maxBones, boneMask, currentTime);
    }

    uint32_t* getEffects()
    {
        return (uint32_t*)((uintptr_t)this + 0xF0);
    }

    Vector* getAbsVelocity()
    {
        return (Vector*)(this + 0x94);
    }

    Vector getBonePosition(int bone) noexcept
    {
        if (matrix3x4 boneMatrices[256]; setupBones(boneMatrices, 256, 256, 0.0f))
            return boneMatrices[bone].origin();
        else
            return Vector{ };
    }

    bool isVisible(const Vector& position = { }) noexcept
    {
        if (!localPlayer)
            return false;

        Trace trace;
        interfaces->engineTrace->traceRay({ getEyePosition(), position.notNull() ? position : getBonePosition(8) }, 0x46004009, { this }, trace);
        return trace.entity == this || trace.fraction > 0.97f;
    }
    
    bool isOtherEnemy(Entity* other) noexcept;

    VarMap* getVarMap() noexcept
    {
        return reinterpret_cast<VarMap*>(this + 0x24);
    }
   
    AnimState* getAnimstate() noexcept
    {
        return *reinterpret_cast<AnimState**>(this + 0x3914);
    }

    float getMaxDesyncAngle() noexcept
    {
        const auto animState = getAnimstate();

        if (!animState)
            return 0.0f;

        float yawModifier = (animState->stopToFullRunningFraction * -0.3f - 0.2f) * std::clamp(animState->footSpeed, 0.0f, 1.0f) + 1.0f;

        if (animState->duckAmount > 0.0f)
            yawModifier += (animState->duckAmount * std::clamp(animState->footSpeed2, 0.0f, 1.0f) * (0.5f - yawModifier));

        return animState->velocitySubtractY * yawModifier;
    }

    bool isInReload() noexcept
    {
        return *reinterpret_cast<bool*>(uintptr_t(&clip()) + 0x41);
    }

    auto getUserId() noexcept
    {
        if (PlayerInfo playerInfo; interfaces->engine->getPlayerInfo(index(), playerInfo))
            return playerInfo.userId;

        return -1;
    }

    std::uint64_t getSteamId() noexcept
    {
        if (PlayerInfo playerInfo; interfaces->engine->getPlayerInfo(index(), playerInfo))
            return playerInfo.xuid;
        return 0;
    }

    void getPlayerName(char(&out)[128]) noexcept;
    [[nodiscard]] std::string getPlayerName() noexcept
    {
        char name[128];
        getPlayerName(name);
        return name;
    }

    bool isThrowing() {
        if (this->isGrenade())
        {
            if (!this->pinPulled())
            {
                float throwtime = this->throwTime();
                if (throwtime > 0)
                    return true;
            }
        }
        return false;
    }

    bool canSee(Entity* other, const Vector& pos) noexcept;
    bool visibleTo(Entity* other) noexcept;

    int getAnimationLayerCount() noexcept
    {
        return *reinterpret_cast<int*>(this + 0x298C);
    }

    AnimationLayer* animOverlays()
    {
        return *reinterpret_cast<AnimationLayer**>(uintptr_t(this) + 0x2980);
    }

    AnimationLayer* getAnimationLayer(int overlay) noexcept
    {
        return &animOverlays()[overlay];
    }

    std::array<float, 24>& pose_parameters()
    {
        return *reinterpret_cast<std::add_pointer_t<std::array<float, 24>>>((uintptr_t)this + netvars->operator[](fnv::hash("CBaseAnimating->m_flPoseParameter")));
    }

    void CreateState(AnimState* state)
    {
        static auto CreateAnimState = reinterpret_cast<void(__thiscall*)(AnimState*, Entity*)>(memory->CreateState);
        if (!CreateAnimState)
            return;

        CreateAnimState(state, this);
    }

    void UpdateState(AnimState* state, Vector angle) {
        if (!state || !angle.notNull())
            return;
        static auto UpdateAnimState = reinterpret_cast<void(__vectorcall*)(void*, void*, float, float, float, void*)>(memory->UpdateState);
        if (!UpdateAnimState)
            return;
        UpdateAnimState(state, nullptr, 0.0f, angle.y, angle.x, nullptr);
    }

    float spawnTime()
    {
        return *(float*)((uintptr_t)this + 0xA370);
    }

    void InvalidateBoneCache()
    {
        static auto invalidate_bone_cache = memory->InvalidateBoneCache;
        reinterpret_cast<void(__fastcall*) (void*)> (invalidate_bone_cache) (this);
    }

    NETVAR(ClientSideAnimation, "CBaseAnimating", "m_bClientSideAnimation", bool)
    NETVAR(body, "CBaseAnimating", "m_nBody", int)
    NETVAR(hitboxSet, "CBaseAnimating", "m_nHitboxSet", int)

    NETVAR(modelIndex, "CBaseEntity", "m_nModelIndex", unsigned)
    NETVAR(origin, "CBaseEntity", "m_vecOrigin", Vector)
    NETVAR_OFFSET(moveType, "CBaseEntity", "m_nRenderMode", 1, MoveType)
    NETVAR(simulationTime, "CBaseEntity", "m_flSimulationTime", float)
    NETVAR(ownerEntity, "CBaseEntity", "m_hOwnerEntity", int)
    NETVAR(spotted, "CBaseEntity", "m_bSpotted", bool)

    NETVAR(weapons, "CBaseCombatCharacter", "m_hMyWeapons", int[64])
    PNETVAR(wearables, "CBaseCombatCharacter", "m_hMyWearables", int)

    NETVAR(viewModel, "CBasePlayer", "m_hViewModel[0]", int)
    NETVAR(fov, "CBasePlayer", "m_iFOV", int)
    NETVAR(fovStart, "CBasePlayer", "m_iFOVStart", int)
    NETVAR(defaultFov, "CBasePlayer", "m_iDefaultFOV", int)
    NETVAR(flags, "CBasePlayer", "m_fFlags", int)
    NETVAR(tickBase, "CBasePlayer", "m_nTickBase", int)
    NETVAR(aimPunchAngle, "CBasePlayer", "m_aimPunchAngle", Vector)
    NETVAR(viewPunchAngle, "CBasePlayer", "m_viewPunchAngle", Vector)
    NETVAR(velocity, "CBasePlayer", "m_vecVelocity[0]", Vector)
    NETVAR(lastPlaceName, "CBasePlayer", "m_szLastPlaceName", char[18])

    NETVAR(armor, "CCSPlayer", "m_ArmorValue", int)
    NETVAR(eyeAngles, "CCSPlayer", "m_angEyeAngles", Vector)
    NETVAR(isScoped, "CCSPlayer", "m_bIsScoped", bool)
    NETVAR(isDefusing, "CCSPlayer", "m_bIsDefusing", bool)
    NETVAR_OFFSET(flashDuration, "CCSPlayer", "m_flFlashMaxAlpha", -8, float)
    NETVAR(flashMaxAlpha, "CCSPlayer", "m_flFlashMaxAlpha", float)
    NETVAR(gunGameImmunity, "CCSPlayer", "m_bGunGameImmunity", bool)
    NETVAR(account, "CCSPlayer", "m_iAccount", int)
    NETVAR(inBombZone, "CCSPlayer", "m_bInBombZone", bool)
    NETVAR(hasDefuser, "CCSPlayer", "m_bHasDefuser", bool)
    NETVAR(hasHelmet, "CCSPlayer", "m_bHasHelmet", bool)
    NETVAR(lby, "CCSPlayer", "m_flLowerBodyYawTarget", float)
    NETVAR(ragdoll, "CCSPlayer", "m_hRagdoll", int)
    NETVAR(shotsFired, "CCSPlayer", "m_iShotsFired", int)
    NETVAR(waitForNoAttack, "CCSPlayer", "m_bWaitForNoAttack", bool)
    NETVAR(readyTime, "CBaseCombatWeapon", "m_flPostponeFireReadyTime", float)
    NETVAR(burstMode, "CBaseCombatWeapon", "m_bBurstMode", bool)
    NETVAR(burstShotRemaining, "CBaseCombatWeapon", "m_iBurstShotsRemaining", int)
    NETVAR(recoilIndex, "CBaseCombatWeapon", "m_flRecoilIndex", float)
    NETVAR(pinPulled, "CBaseCSGrenade", "m_bPinPulled", bool);
    NETVAR(throwTime, "CBaseCSGrenade", "m_fThrowTime", float);

    NETVAR(viewModelIndex, "CBaseCombatWeapon", "m_iViewModelIndex", int)
    NETVAR(worldModelIndex, "CBaseCombatWeapon", "m_iWorldModelIndex", int)
    NETVAR(worldDroppedModelIndex, "CBaseCombatWeapon", "m_iWorldDroppedModelIndex", int)
    NETVAR(weaponWorldModel, "CBaseCombatWeapon", "m_hWeaponWorldModel", int)
    NETVAR(clip, "CBaseCombatWeapon", "m_iClip1", int)
    NETVAR(reserveAmmoCount, "CBaseCombatWeapon", "m_iPrimaryReserveAmmoCount", int)
    NETVAR(nextPrimaryAttack, "CBaseCombatWeapon", "m_flNextPrimaryAttack", float)
    NETVAR(nextSecondaryAttack, "CBaseCombatWeapon", "m_flNextSecondaryAttack", float)

    NETVAR(nextAttack, "CBaseCombatCharacter", "m_flNextAttack", float)

    NETVAR(accountID, "CBaseAttributableItem", "m_iAccountID", int)
    NETVAR(itemDefinitionIndex, "CBaseAttributableItem", "m_iItemDefinitionIndex", short)
    NETVAR(itemDefinitionIndex2, "CBaseAttributableItem", "m_iItemDefinitionIndex", WeaponId)
    NETVAR(itemIDHigh, "CBaseAttributableItem", "m_iItemIDHigh", int)
    NETVAR(entityQuality, "CBaseAttributableItem", "m_iEntityQuality", int)
    NETVAR(customName, "CBaseAttributableItem", "m_szCustomName", char[32])
    NETVAR(fallbackPaintKit, "CBaseAttributableItem", "m_nFallbackPaintKit", unsigned)
    NETVAR(fallbackSeed, "CBaseAttributableItem", "m_nFallbackSeed", unsigned)
    NETVAR(fallbackWear, "CBaseAttributableItem", "m_flFallbackWear", float)
    NETVAR(fallbackStatTrak, "CBaseAttributableItem", "m_nFallbackStatTrak", unsigned)
    NETVAR(initialized, "CBaseAttributableItem", "m_bInitialized", bool)

    NETVAR(owner, "CBaseViewModel", "m_hOwner", int)
    NETVAR(weapon, "CBaseViewModel", "m_hWeapon", int)

    NETVAR(c4StartedArming, "CC4", "m_bStartedArming", bool)

    NETVAR(tabletReceptionIsBlocked, "CTablet", "m_bTabletReceptionIsBlocked", bool)
    
    NETVAR(droneTarget, "CDrone", "m_hMoveToThisEntity", int)

    NETVAR(thrower, "CBaseGrenade", "m_hThrower", int)
        
    NETVAR(mapHasBombTarget, "CCSGameRulesProxy", "m_bMapHasBombTarget", bool)
    NETVAR(freezePeriod, "CCSGameRulesProxy", "m_bFreezePeriod", bool)
    NETVAR(isValveDS, "CCSGameRulesProxy", "m_bIsValveDS", bool)

    NETVAR(fireXDelta, "CInferno", "m_fireXDelta", int[100])
    NETVAR(fireYDelta, "CInferno", "m_fireYDelta", int[100])
    NETVAR(fireZDelta, "CInferno", "m_fireZDelta", int[100])
    NETVAR(fireIsBurning, "CInferno", "m_bFireIsBurning", bool[100])
    NETVAR(fireCount, "CInferno", "m_fireCount", int)
        
    bool isFlashed() noexcept
    {
        return flashDuration() > 75.0f;
    }

    bool grenadeExploded() noexcept
    {
        return *reinterpret_cast<bool*>(this + 0x29E8);
    }
};

class PlantedC4 : public Entity {
public:
    NETVAR(c4BlowTime, "CPlantedC4", "m_flC4Blow", float)
    NETVAR(c4TimerLength, "CPlantedC4", "m_flTimerLength", float)
    NETVAR(c4BombSite, "CPlantedC4", "m_nBombSite", int)
    NETVAR(c4Ticking, "CPlantedC4", "m_bBombTicking", bool)
    NETVAR(c4DefuseCountDown, "CPlantedC4", "m_flDefuseCountDown", float)
    NETVAR(c4DefuseLength, "CPlantedC4", "m_flDefuseLength", float)
    NETVAR(c4Defuser, "CPlantedC4", "m_hBombDefuser", int)
};

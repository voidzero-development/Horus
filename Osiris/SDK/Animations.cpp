#include "Animations.h"

#include "../Memory.h"
#include "../MemAlloc.h"
#include "../Interfaces.h"

#include "../SDK/LocalPlayer.h"
#include "../SDK/Cvar.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/Entity.h"
#include "../SDK/UserCmd.h"
#include "../SDK/ConVar.h"
#include "../SDK/Input.h"

Animations::Data Animations::data;

void Animations::update(UserCmd* cmd, bool& sendPacket) noexcept
{
    if (!localPlayer || !localPlayer->isAlive())
        return;
    data.viewAngles = cmd->viewangles;
    data.sendPacket = sendPacket;
}

void Animations::fake() noexcept
{
    static AnimState* fakeanimstate = nullptr;
    static bool updatefakeanim = true;
    static bool initfakeanim = true;
    static float spawnTime = 0.f;

    if (!interfaces->engine->isInGame())
    {
        updatefakeanim = true;
        initfakeanim = true;
        spawnTime = 0.f;
        fakeanimstate = nullptr;
        return;
    }

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (spawnTime != localPlayer.get()->spawnTime() || updatefakeanim)
    {
        spawnTime = localPlayer.get()->spawnTime();
        initfakeanim = false;
        updatefakeanim = false;
    }

    if (!initfakeanim)
    {
        fakeanimstate = static_cast<AnimState*>(memory->memAlloc->alloc(sizeof(AnimState)));

        if (fakeanimstate != nullptr)
            localPlayer.get()->createState(fakeanimstate);

        initfakeanim = true;
    }
    if (data.sendPacket)
    {
        std::array<AnimationLayer, 15> networked_layers;

        std::memcpy(&networked_layers, localPlayer.get()->animOverlays(), sizeof(AnimationLayer) * localPlayer->getAnimationLayerCount());
        auto backup_abs_angles = localPlayer.get()->getAbsAngle();
        auto backup_poses = localPlayer.get()->poseParameters();

        *(uint32_t*)((uintptr_t)localPlayer.get() + 0xA68) = 0;

        localPlayer.get()->updateState(fakeanimstate, data.viewangles);
        localPlayer.get()->invalidateBoneCache();
        memory->setAbsAngle(localPlayer.get(), Vector{ 0, fakeanimstate->goalFeetYaw, 0 });
        std::memcpy(localPlayer.get()->animOverlays(), &networked_layers, sizeof(AnimationLayer) * localPlayer->getAnimationLayerCount());
        localPlayer.get()->getAnimationLayer(12)->weight = std::numeric_limits<float>::epsilon();
        data.gotMatrix = localPlayer.get()->setupBones(data.fakeMatrix, 256, 0x7FF00, memory->globalVars->currenttime);
        const auto origin = localPlayer.get()->getRenderOrigin();
        if (data.gotMatrix)
        {
            for (auto& i : data.fakeMatrix)
            {
                i[0][3] -= origin.x;
                i[1][3] -= origin.y;
                i[2][3] -= origin.z;
            }
        }
        std::memcpy(localPlayer.get()->animOverlays(), &networked_layers, sizeof(AnimationLayer) * localPlayer->getAnimationLayerCount());
        localPlayer.get()->poseParameters() = backup_poses;
        memory->setAbsAngle(localPlayer.get(), Vector{ 0,backup_abs_angles.y,0 });
    }
}

void Animations::real() noexcept
{
    static auto jigglebones = interfaces->cvar->findVar("r_jiggle_bones");
    jigglebones->setValue(0);
    if (!localPlayer)
        return;

    if (!localPlayer->isAlive())
    {
        localPlayer.get()->clientSideAnimation() = true;
        return;
    }

    if (!memory->input->isCameraInThirdPerson) {
        localPlayer.get()->clientSideAnimation() = true;
        localPlayer.get()->updateClientSideAnimation();
        localPlayer.get()->clientSideAnimation() = false;
        return;
    }

    static auto backup_poses = localPlayer.get()->poseParameters();
    static auto backup_abs = localPlayer.get()->getAnimstate()->goalFeetYaw;

    static std::array<AnimationLayer, 15> networked_layers;

    while (localPlayer.get()->getAnimstate()->lastClientSideAnimationUpdateFramecount == memory->globalVars->framecount)
        localPlayer.get()->getAnimstate()->lastClientSideAnimationUpdateFramecount -= 1;

    static int old_tick = 0;
    if (old_tick != memory->globalVars->tickCount)
    {
        old_tick = memory->globalVars->tickCount;
        std::memcpy(&networked_layers, localPlayer.get()->animOverlays(), sizeof(AnimationLayer) * localPlayer->getAnimationLayerCount());
        localPlayer.get()->clientSideAnimation() = true;
        localPlayer.get()->updateState(localPlayer->getAnimstate(), data.viewangles);
        localPlayer.get()->updateClientSideAnimation();
        localPlayer.get()->clientSideAnimation() = false;
        if (data.sendPacket)
        {
            backup_poses = localPlayer.get()->poseParameters();
            backup_abs = localPlayer.get()->getAnimstate()->goalFeetYaw;
        }
    }
    localPlayer.get()->getAnimstate()->feetYawRate = 0.f;
    memory->setAbsAngle(localPlayer.get(), Vector{ 0,backup_abs,0 });
    std::memcpy(localPlayer.get()->animOverlays(), &networked_layers, sizeof(AnimationLayer) * localPlayer->getAnimationLayerCount());
    localPlayer.get()->poseParameters() = backup_poses;
}
#define NOMINMAX
#include <functional>
#include <string>

#include "imgui/imgui.h"

#ifdef _WIN32
#include <intrin.h>
#include <Windows.h>
#include <Psapi.h>

#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"

#include "MinHook/MinHook.h"
#elif __linux__
#include <sys/mman.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include "imgui/GL/gl3w.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_impl_opengl3.h"
#endif

#include "Config.h"
#include "EventListener.h"
#include "GameData.h"
#include "GUI.h"
#include "Hooks.h"
#include "ExtraHooks.h"
#include "Interfaces.h"
#include "Memory.h"

#include "Hacks/Animations.h"
#include "Hacks/AntiAim.h"
#include "Hacks/Backtrack.h"
#include "Hacks/Chams.h"
#include "Hacks/EnginePrediction.h"
#include "Hacks/StreamProofESP.h"
#include "Hacks/Glow.h"
#include "Hacks/GrenadePrediction.h"
#include "Hacks/Legitbot.h"
#include "Hacks/Misc.h"
#include "Hacks/Ragebot.h"
#include "Hacks/SkinChanger.h"
#include "Hacks/Triggerbot.h"
#include "Hacks/Visuals.h"

#include "SDK/ConVar.h"
#include "SDK/Engine.h"
#include "SDK/Entity.h"
#include "SDK/EntityList.h"
#include "SDK/FrameStage.h"
#include "SDK/GameEvent.h"
#include "SDK/GameMovement.h"
#include "SDK/GameUI.h"
#include "SDK/GlobalVars.h"
#include "SDK/InputSystem.h"
#include "SDK/MaterialSystem.h"
#include "SDK/ModelRender.h"
#include "SDK/Panel.h"
#include "SDK/Platform.h"
#include "SDK/RenderContext.h"
#include "SDK/SoundInfo.h"
#include "SDK/SoundEmitter.h"
#include "SDK/StudioRender.h"
#include "SDK/Surface.h"
#include "SDK/UserCmd.h"

#ifdef _WIN32

static LRESULT __stdcall wndProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
    [[maybe_unused]] static const auto once = [](HWND window) noexcept {
        netvars = std::make_unique<Netvars>();
        eventListener = std::make_unique<EventListener>();

        ImGui::CreateContext();
        ImGui_ImplWin32_Init(window);
        config = std::make_unique<Config>("Osiris");
        gui = std::make_unique<GUI>();

        hooks->install();

        return true;
    }(window);

    LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    ImGui_ImplWin32_WndProcHandler(window, msg, wParam, lParam);

    interfaces->inputSystem->enableInput(!gui->isOpen());

    return CallWindowProcW(hooks->originalWndProc, window, msg, wParam, lParam);
}

static HRESULT __stdcall present(IDirect3DDevice9* device, const RECT* src, const RECT* dest, HWND windowOverride, const RGNDATA* dirtyRegion) noexcept
{
    [[maybe_unused]] static bool imguiInit{ ImGui_ImplDX9_Init(device) };

    if (config->loadScheduledFonts())
        ImGui_ImplDX9_DestroyFontsTexture();

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    StreamProofESP::render();
    GrenadePrediction::draw();
    Misc::antiAimLines();
    Misc::purchaseList();
    Misc::drawOffscreenEnemies(ImGui::GetBackgroundDrawList());
    Misc::drawBombTimer();
    Misc::spectatorList();
    Visuals::hitMarker(nullptr, ImGui::GetBackgroundDrawList());
    Visuals::drawMolotovHull(ImGui::GetBackgroundDrawList());
    Misc::watermark();

    Legitbot::updateInput();
    Ragebot::updateInput();
    Visuals::updateInput();
    StreamProofESP::updateInput();
    Misc::updateInput();
    Triggerbot::updateInput();
    Chams::updateInput();
    Glow::updateInput();

    gui->handleToggle();

    if (gui->isOpen())
        gui->render();

    ImGui::EndFrame();
    ImGui::Render();

    if (device->BeginScene() == D3D_OK) {
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        device->EndScene();
    }

    GameData::clearUnusedAvatars();

    return hooks->originalPresent(device, src, dest, windowOverride, dirtyRegion);
}

static HRESULT __stdcall reset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params) noexcept
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    SkinChanger::clearItemIconTextures();
    GameData::clearTextures();
    return hooks->originalReset(device, params);
}

#endif

static int __fastcall sendDatagram(NetworkChannel* network, void* edx, void* datagram)
{
    auto original = hooks->networkChannel.getOriginal<int, void*>(46, datagram);
    if (!config->misc.fakeLatency.enabled || datagram || !interfaces->engine->isInGame())
    {
        return original(network, datagram);
    }
    int instate = network->inReliableState;
    int insequencenr = network->inSequenceNr;

    float delta = std::max(0.f, std::clamp(config->misc.fakeLatency.amount / 1000.f, 0.f, interfaces->cvar->findVar("sv_maxunlag")->getFloat()) - network->getLatency(0));

    Backtrack::addLatencyToNetwork(network, delta);

    int result = original(network, datagram);

    network->inReliableState = instate;
    network->inSequenceNr = insequencenr;

    return result;
}

static bool __fastcall sendNetworkMessage(NetworkChannel* network, void* edx, NetworkMessage& msg, bool forceReliable, bool voice)
{
    auto original = hooks->networkChannel.getOriginal<bool, NetworkMessage&, bool, bool>(40, msg, forceReliable, voice);

    if (msg.getType() == 14 && config->misc.pureBypass)
        return false;

    return original(network, msg, forceReliable, voice);
}

static bool __STDCALL createMove(LINUX_ARGS(void* thisptr,) float inputSampleTime, UserCmd* cmd) noexcept
{
    auto result = hooks->clientMode.callOriginal<bool, IS_WIN32() ? 24 : 25>(inputSampleTime, cmd);

    if (!cmd->commandNumber)
        return result;

#ifdef _WIN32
    uintptr_t* framePointer;
    __asm mov framePointer, ebp;
    bool& sendPacket = *reinterpret_cast<bool*>(*framePointer - 0x1C);
#else
    bool dummy;
    bool& sendPacket = dummy;
#endif

    static auto previousViewAngles{ cmd->viewangles };
    const auto oldCurrentViewAngles{ cmd->viewangles };

    float angle = Misc::rageStrafer(cmd, oldCurrentViewAngles);
    const auto currentViewAngles{ cmd->viewangles };
    memory->globalVars->serverTime(cmd);
    Misc::antiAfkKick(cmd);
    Misc::fastStop(cmd);
    Misc::prepareRevolver(cmd);
    Visuals::removeShadows();
    Misc::runReportbot();
    Misc::bunnyHop(cmd);
    Misc::legitStrafer(cmd);
    Misc::removeCrouchCooldown(cmd);
    Misc::autoPistol(cmd);
    Misc::clanTag();
    Misc::stealNames();
    Misc::revealRanks(cmd);
    Misc::fixTabletSignal();
    Misc::slowwalk(cmd);
    Misc::deathmatchGod();
    Legitbot::autoStop(cmd);
    Ragebot::autoStop(cmd);

    extraHook.init();

    static void* oldPointer = nullptr;
    auto network = interfaces->engine->getNetworkChannel();
    if (oldPointer != network && network && interfaces->engine->isInGame() && interfaces->engine->isConnected())
    {
        oldPointer = network;
        Backtrack::updateIncomingSequences(true);
        hooks->networkChannel.init(network);
        hooks->networkChannel.hookAt(40, sendNetworkMessage);
        hooks->networkChannel.hookAt(46, sendDatagram);
    }
    Backtrack::updateIncomingSequences();

    cmd->viewangles.y -= angle;

    GrenadePrediction::run(cmd);

    EnginePrediction::run(cmd);

    Triggerbot::run(cmd);
    Legitbot::run(cmd);
    Ragebot::run(cmd);
    Backtrack::run(cmd);

    Resolver::updateShots(cmd);

    Misc::edgejump(cmd);
    Misc::moonwalk(cmd);
    Misc::fastPlant(cmd);
    Misc::forceCrosshair();
    Misc::recoilCrosshair();
    Misc::blockBot(cmd);

    AntiAim::fakeLag(cmd, sendPacket);
    AntiAim::fakeDuck(cmd);
    AntiAim::run(cmd, previousViewAngles, currentViewAngles, sendPacket);

    auto viewAnglesDelta{ cmd->viewangles - previousViewAngles };
    viewAnglesDelta.normalize();
    float maxAngleDelta{ Misc::shouldAimStep() ? 39.f : 255.f };
    viewAnglesDelta.x = std::clamp(viewAnglesDelta.x, -maxAngleDelta, maxAngleDelta);
    viewAnglesDelta.y = std::clamp(viewAnglesDelta.y, -maxAngleDelta, maxAngleDelta);

    cmd->viewangles = previousViewAngles + viewAnglesDelta;

    cmd->viewangles.normalize();
    Misc::fixMovement(cmd, currentViewAngles.y);
    Misc::slideFix(cmd, currentViewAngles.y);

    cmd->viewangles.x = std::clamp(cmd->viewangles.x, -89.0f, 89.0f);
    cmd->viewangles.y = std::clamp(cmd->viewangles.y, -180.0f, 180.0f);
    cmd->viewangles.z = 0.0f;
    cmd->forwardmove = std::clamp(cmd->forwardmove, -450.0f, 450.0f);
    cmd->sidemove = std::clamp(cmd->sidemove, -450.0f, 450.0f);

    previousViewAngles = cmd->viewangles;

    Animations::update(cmd, sendPacket);
    Animations::fake();

    return false;
}

static void __STDCALL doPostScreenEffects(LINUX_ARGS(void* thisptr,) void* param) noexcept
{
    if (interfaces->engine->isInGame()) {
        Visuals::thirdPerson();
        Misc::inverseRagdollGravity();
        Visuals::reduceFlashEffect();
        Visuals::updateBrightness();
        Visuals::remove3dSky();
        Glow::render();
    }
    hooks->clientMode.callOriginal<void, IS_WIN32() ? 44 : 45>(param);
}

static float __STDCALL getViewModelFov(LINUX_ARGS(void* thisptr)) noexcept
{
    float additionalFov = static_cast<float>(config->visuals.viewmodelFov);
    if (localPlayer) {
        if (const auto activeWeapon = localPlayer->getActiveWeapon(); activeWeapon && activeWeapon->getClientClass()->classId == ClassId::Tablet)
            additionalFov = 0.0f;
    }

    return hooks->clientMode.callOriginal<float, IS_WIN32() ? 35 : 36>() + additionalFov;
}

static void __STDCALL drawModelExecute(LINUX_ARGS(void* thisptr,) void* ctx, void* state, const ModelRenderInfo& info, matrix3x4* customBoneToWorld) noexcept
{
    if (interfaces->studioRender->isForcedMaterialOverride())
        return hooks->modelRender.callOriginal<void, 21>(ctx, state, std::cref(info), customBoneToWorld);

    if (Visuals::removeHands(info.model->name) || Visuals::removeSleeves(info.model->name) || Visuals::removeWeapons(info.model->name))
        return;

    static Chams chams;
    if (!chams.render(ctx, state, info, customBoneToWorld))
        hooks->modelRender.callOriginal<void, 21>(ctx, state, std::cref(info), customBoneToWorld);

    interfaces->studioRender->forcedMaterialOverride(nullptr);
}

static bool __FASTCALL svCheatsGetBool(void* _this) noexcept
{
    if (uintptr_t(RETURN_ADDRESS()) == memory->cameraThink && config->visuals.thirdperson)
        return true;

    return hooks->svCheats.getOriginal<bool, IS_WIN32() ? 13 : 16>()(_this);
}

static void __STDCALL frameStageNotify(LINUX_ARGS(void* thisptr,) FrameStage stage) noexcept
{
    [[maybe_unused]] static auto backtrackInit = (Backtrack::init(), false);

    if (interfaces->engine->isConnected() && !interfaces->engine->isInGame())
        Misc::changeName(true, nullptr, 0.0f);

    for (size_t i = 0; i < config->ragebot.size(); i++)
    {
        if (config->ragebot[i].enabled && config->legitbot[i].enabled)
            config->ragebot[i].enabled = false;
    }

    if (stage == FrameStage::START)
        GameData::update();

    if (stage == FrameStage::RENDER_START) {
        Misc::preserveKillfeed();
        Misc::disablePanoramablur();
        Visuals::colorWorld();
        Misc::fakePrime();
        Animations::players();
        Animations::real();
        Misc::forceRelayCluster();
    }
    if (interfaces->engine->isInGame()) {
        Visuals::skybox(stage);
        Visuals::removeBlur(stage);
        Misc::oppositeHandKnife(stage);
        Misc::viewmodelChanger(stage);
        //Misc::chatSpammer();
        Visuals::removeGrass(stage);
        Visuals::modifySmoke(stage);
        Visuals::playerModel(stage);
        Visuals::disablePostProcessing(stage);
        Visuals::removeVisualRecoil(stage);
        Visuals::applyZoom(stage);
        Misc::fixAnimationLOD(stage);
        Backtrack::update(stage);
        SkinChanger::run(stage);
    }
    hooks->client.callOriginal<void, 37>(stage);
}

static void __STDCALL emitSound(LINUX_ARGS(void* thisptr,) void* filter, int entityIndex, int channel, const char* soundEntry, unsigned int soundEntryHash, const char* sample, float volume, int seed, int soundLevel, int flags, int pitch, const Vector& origin, const Vector& direction, void* utlVecOrigins, bool updatePositions, float soundtime, int speakerentity, void* soundParams) noexcept
{
    auto modulateVolume = [&](int(*get)(int)) {
        if (const auto entity = interfaces->entityList->getEntity(entityIndex); localPlayer && entity && entity->isPlayer()) {
            if (entityIndex == localPlayer->index())
                volume *= get(0) / 100.0f;
            else if (!entity->isOtherEnemy(localPlayer.get()))
                volume *= get(1) / 100.0f;
            else
                volume *= get(2) / 100.0f;
        }
    };

    modulateVolume([](int index) { return config->sound.players[index].masterVolume; });

    if (strstr(soundEntry, "Weapon") && strstr(soundEntry, "Single")) {
        modulateVolume([](int index) { return config->sound.players[index].weaponVolume; });
    }
    
    Misc::autoAccept(soundEntry);

    volume = std::clamp(volume, 0.0f, 1.0f);
    hooks->sound.callOriginal<void, IS_WIN32() ? 5 : 6>(filter, entityIndex, channel, soundEntry, soundEntryHash, sample, volume, seed, soundLevel, flags, pitch, std::cref(origin), std::cref(direction), utlVecOrigins, updatePositions, soundtime, speakerentity, soundParams);
}

static bool __STDCALL shouldDrawFog(LINUX_ARGS(void* thisptr)) noexcept
{
#ifdef _WIN32
    if constexpr (std::is_same_v<HookType, MinHook>) {
#ifdef _DEBUG
    // Check if we always get the same return address
    if (*static_cast<std::uint32_t*>(_ReturnAddress()) == 0x6274C084) {
        static const auto returnAddress = std::uintptr_t(_ReturnAddress());
        assert(returnAddress == std::uintptr_t(_ReturnAddress()));
    }
#endif

    if (*static_cast<std::uint32_t*>(_ReturnAddress()) != 0x6274C084)
        return hooks->clientMode.callOriginal<bool, 17>();
    }
#endif
    
    return !config->visuals.noFog;
}

static bool __STDCALL shouldDrawViewModel(LINUX_ARGS(void* thisptr)) noexcept
{
    if (config->visuals.zoom && localPlayer && localPlayer->fov() < 45 && localPlayer->fovStart() < 45)
        return false;
    return hooks->clientMode.callOriginal<bool, IS_WIN32() ? 27 : 28>();
}

static void __STDCALL lockCursor() noexcept
{
    if (gui->isOpen())
        return interfaces->surface->unlockCursor();
    return hooks->surface.callOriginal<void, 67>();
}

static void __STDCALL setDrawColor(LINUX_ARGS(void* thisptr,) int r, int g, int b, int a) noexcept
{
    if (config->visuals.noScopeOverlay && (RETURN_ADDRESS() == memory->scopeDust || RETURN_ADDRESS() == memory->scopeArc))
        a = 0;
    hooks->surface.callOriginal<void, IS_WIN32() ? 15 : 14>(r, g, b, a);
}

struct ViewSetup {
    std::byte pad[176];
    float fov;
    float fovViewmodel;
    Vector origin;
    Vector angles;
    std::byte pad1[4];
    float farZ;
};

constexpr void fakeDuckFix(ViewSetup* setup) noexcept {
    if (localPlayer && localPlayer->isAlive() && AntiAim::fakeDucking && localPlayer->flags() & 1)
        setup->origin.z = localPlayer->getAbsOrigin().z + interfaces->gameMovement->getPlayerViewOffset(false).z;
}

static void __STDCALL overrideView(LINUX_ARGS(void* thisptr,) ViewSetup* setup) noexcept
{
    if (localPlayer) {
        Entity* target = localPlayer->isAlive() ? localPlayer.get() : localPlayer->getObserverTarget();

        if (config->misc.viewmodelChanger.enabled && config->misc.viewmodelChanger.roll) {

            if (target && target->isAlive() && !target->isScoped()) {
                const auto viewModel = interfaces->entityList->getEntityFromHandle(target->viewModel());
                if (viewModel)
                    memory->setAbsAngle(viewModel, setup->angles + Vector{ 0.f, 0.f, (float)config->misc.viewmodelChanger.roll });
            }
        }
    }

    if (localPlayer && !localPlayer->isScoped())
        setup->fov += config->visuals.fov;
    setup->farZ += config->visuals.farZ * 10;
    fakeDuckFix(setup);
    hooks->clientMode.callOriginal<void, IS_WIN32() ? 18 : 19>(setup);
}

struct RenderableInfo {
    Entity* renderable;
    std::byte pad[18];
    uint16_t flags;
    uint16_t flags2;
};

static int __STDCALL listLeavesInBox(const Vector& mins, const Vector& maxs, unsigned short* list, int listMax) noexcept
{
#ifdef _WIN32
    if (std::uintptr_t(_ReturnAddress()) == memory->listLeaves) {
        if (const auto info = *reinterpret_cast<RenderableInfo**>(std::uintptr_t(_AddressOfReturnAddress()) + 0x14); info && info->renderable) {
            if (const auto ent = VirtualMethod::call<Entity*, 7>(info->renderable - 4); ent && ent->isPlayer()) {
                if (config->misc.disableModelOcclusion) {
                    // FIXME: sometimes players are rendered above smoke, maybe sort render list?
                    info->flags &= ~0x100;
                    info->flags2 |= 0x40;

                    constexpr float maxCoord = 16384.0f;
                    constexpr float minCoord = -maxCoord;
                    constexpr Vector min{ minCoord, minCoord, minCoord };
                    constexpr Vector max{ maxCoord, maxCoord, maxCoord };
                    return hooks->bspQuery.callOriginal<int, 6>(std::cref(min), std::cref(max), list, listMax);
                }
            }
        }
    }
#endif
    return hooks->bspQuery.callOriginal<int, 6>(std::cref(mins), std::cref(maxs), list, listMax);
}

static int __FASTCALL dispatchSound(SoundInfo& soundInfo) noexcept
{
    if (const char* soundName = interfaces->soundEmitter->getSoundName(soundInfo.soundIndex)) {
        auto modulateVolume = [&soundInfo](int(*get)(int)) {
            if (auto entity{ interfaces->entityList->getEntity(soundInfo.entityIndex) }; entity && entity->isPlayer()) {
                if (localPlayer && soundInfo.entityIndex == localPlayer->index())
                    soundInfo.volume *= get(0) / 100.0f;
                else if (!entity->isOtherEnemy(localPlayer.get()))
                    soundInfo.volume *= get(1) / 100.0f;
                else
                    soundInfo.volume *= get(2) / 100.0f;
            }
        };

        modulateVolume([](int index) { return config->sound.players[index].masterVolume; });

        if (!strcmp(soundName, "Player.DamageHelmetFeedback"))
            modulateVolume([](int index) { return config->sound.players[index].headshotVolume; });
        else if (strstr(soundName, "Step"))
            modulateVolume([](int index) { return config->sound.players[index].footstepVolume; });
        else if (strstr(soundName, "Chicken"))
            soundInfo.volume *= config->sound.chickenVolume / 100.0f;
    }
    soundInfo.volume = std::clamp(soundInfo.volume, 0.0f, 1.0f);
    return hooks->originalDispatchSound(soundInfo);
}

static void __STDCALL render2dEffectsPreHud(LINUX_ARGS(void* thisptr,) void* viewSetup) noexcept
{
    Visuals::applyScreenEffects();
    Visuals::hitEffect();
    hooks->viewRender.callOriginal<void, IS_WIN32() ? 39 : 40>(viewSetup);
}

static const DemoPlaybackParameters* __STDCALL getDemoPlaybackParameters(LINUX_ARGS(void* thisptr)) noexcept
{
    const auto params = hooks->engine.callOriginal<const DemoPlaybackParameters*, IS_WIN32() ? 218 : 219>();

    if (params && config->misc.revealSuspect && RETURN_ADDRESS() != memory->demoFileEndReached) {
        static DemoPlaybackParameters customParams;
        customParams = *params;
        customParams.anonymousPlayerIdentity = false;
        return &customParams;
    }

    return params;
}

static bool __STDCALL isPlayingDemo(LINUX_ARGS(void* thisptr)) noexcept
{
    if (config->misc.revealMoney && RETURN_ADDRESS() == memory->demoOrHLTV && *reinterpret_cast<std::uintptr_t*>(FRAME_ADDRESS() + (IS_WIN32() ? 8 : 24)) == memory->money)
        return true;

    return hooks->engine.callOriginal<bool, 82>();
}

static void __STDCALL updateColorCorrectionWeights(LINUX_ARGS(void* thisptr)) noexcept
{
    hooks->clientMode.callOriginal<void, IS_WIN32() ? 58 : 61>();

    if (const auto& cfg = config->visuals.colorCorrection; cfg.enabled) {
        *reinterpret_cast<float*>(std::uintptr_t(memory->clientMode) + (IS_WIN32() ? 0x498 : 0x900)) = cfg.blue;
        *reinterpret_cast<float*>(std::uintptr_t(memory->clientMode) + (IS_WIN32() ? 0x4A0 : 0x910)) = cfg.red;
        *reinterpret_cast<float*>(std::uintptr_t(memory->clientMode) + (IS_WIN32() ? 0x4A8 : 0x920)) = cfg.mono;
        *reinterpret_cast<float*>(std::uintptr_t(memory->clientMode) + (IS_WIN32() ? 0x4B0 : 0x930)) = cfg.saturation;
        *reinterpret_cast<float*>(std::uintptr_t(memory->clientMode) + (IS_WIN32() ? 0x4C0 : 0x950)) = cfg.ghost;
        *reinterpret_cast<float*>(std::uintptr_t(memory->clientMode) + (IS_WIN32() ? 0x4C8 : 0x960)) = cfg.green;
        *reinterpret_cast<float*>(std::uintptr_t(memory->clientMode) + (IS_WIN32() ? 0x4D0 : 0x970)) = cfg.yellow;
    }

    if (config->visuals.noScopeOverlay)
        *memory->vignette = 0.0f;
}

static float __STDCALL getScreenAspectRatio(LINUX_ARGS(void* thisptr,) int width, int height) noexcept
{
    if (config->misc.aspectratio)
        return config->misc.aspectratio;
    return hooks->engine.callOriginal<float, 101>(width, height);
}

static bool __FASTCALL isConnected() noexcept
{
    if (static_cast<int*>(_ReturnAddress()) == memory->isLoadOutAvailable && config->misc.unlockInventory)
        return false;
    return hooks->engine.callOriginal<bool, 27>();
}

static void __STDCALL renderSmokeOverlay(LINUX_ARGS(void* thisptr,) bool update) noexcept
{
    if (config->visuals.noSmoke || config->visuals.wireframeSmoke)
        *reinterpret_cast<float*>(std::uintptr_t(memory->viewRender) + (IS_WIN32() ? 0x588 : 0x648)) = 0.0f;
    else
        hooks->viewRender.callOriginal<void, IS_WIN32() ? 41 : 42>(update);
}

#ifdef _WIN32

Hooks::Hooks(HMODULE moduleHandle) noexcept
{
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

    this->moduleHandle = moduleHandle;

    // interfaces and memory shouldn't be initialized in wndProc because they show MessageBox on error which would cause deadlock
    interfaces = std::make_unique<const Interfaces>();
    memory = std::make_unique<const Memory>();

    window = FindWindowW(L"Valve001", nullptr);
    originalWndProc = WNDPROC(SetWindowLongPtrW(window, GWLP_WNDPROC, LONG_PTR(wndProc)));
}

#else

static void swapWindow(SDL_Window* window) noexcept
{
    static const auto _ = ImGui_ImplSDL2_InitForOpenGL(window, nullptr);
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);

    ImGui::NewFrame();

    if (const auto& displaySize = ImGui::GetIO().DisplaySize; displaySize.x > 0.0f && displaySize.y > 0.0f) {
        StreamProofESP::render();
        Misc::purchaseList();
        Misc::drawOffscreenEnemies(ImGui::GetBackgroundDrawList());
        Misc::drawBombTimer();
        Misc::spectatorList();
        Visuals::hitMarker(nullptr, ImGui::GetBackgroundDrawList());
        Visuals::drawMolotovHull(ImGui::GetBackgroundDrawList());
        Misc::watermark();

        Aimbot::updateInput();
        Visuals::updateInput();
        StreamProofESP::updateInput();
        Misc::updateInput();
        Triggerbot::updateInput();
        Chams::updateInput();
        Glow::updateInput();

        gui->handleToggle();

        if (gui->isOpen())
            gui->render();
    }

    ImGui::EndFrame();
    ImGui::Render();

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    GameData::clearUnusedAvatars();

    hooks->swapWindow(window);
}

#endif

void Hooks::install() noexcept
{
#ifdef _WIN32
    originalPresent = **reinterpret_cast<decltype(originalPresent)**>(memory->present);
    **reinterpret_cast<decltype(present)***>(memory->present) = present;
    originalReset = **reinterpret_cast<decltype(originalReset)**>(memory->reset);
    **reinterpret_cast<decltype(reset)***>(memory->reset) = reset;

    if constexpr (std::is_same_v<HookType, MinHook>)
        MH_Initialize();
#else
    gl3wInit();
    ImGui_ImplOpenGL3_Init();

    swapWindow = *reinterpret_cast<decltype(swapWindow)*>(memory->swapWindow);
    *reinterpret_cast<decltype(::swapWindow)**>(memory->swapWindow) = ::swapWindow;

#endif
    
#ifdef _WIN32
    bspQuery.init(interfaces->engine->getBSPTreeQuery());
#endif

    client.init(interfaces->client);
    client.hookAt(37, frameStageNotify);

    clientMode.init(memory->clientMode);
    clientMode.hookAt(IS_WIN32() ? 17 : 18, shouldDrawFog);
    clientMode.hookAt(IS_WIN32() ? 18 : 19, overrideView);
    clientMode.hookAt(IS_WIN32() ? 24 : 25, createMove);
    clientMode.hookAt(IS_WIN32() ? 27 : 28, shouldDrawViewModel);
    clientMode.hookAt(IS_WIN32() ? 35 : 36, getViewModelFov);
    clientMode.hookAt(IS_WIN32() ? 44 : 45, doPostScreenEffects);
    clientMode.hookAt(IS_WIN32() ? 58 : 61, updateColorCorrectionWeights);

    engine.init(interfaces->engine);
    engine.hookAt(27, isConnected);
    engine.hookAt(82, isPlayingDemo);
    engine.hookAt(101, getScreenAspectRatio);
    engine.hookAt(IS_WIN32() ? 218 : 219, getDemoPlaybackParameters);

    modelRender.init(interfaces->modelRender);
    modelRender.hookAt(21, drawModelExecute);

    sound.init(interfaces->sound);
    sound.hookAt(IS_WIN32() ? 5 : 6, emitSound);

    surface.init(interfaces->surface);
    surface.hookAt(IS_WIN32() ? 15 : 14, setDrawColor);
    
    svCheats.init(interfaces->cvar->findVar("sv_cheats"));
    svCheats.hookAt(IS_WIN32() ? 13 : 16, svCheatsGetBool);

    viewRender.init(memory->viewRender);
    viewRender.hookAt(IS_WIN32() ? 39 : 40, render2dEffectsPreHud);
    viewRender.hookAt(IS_WIN32() ? 41 : 42, renderSmokeOverlay);

#ifdef _WIN32
    if (DWORD oldProtection; VirtualProtect(memory->dispatchSound, 4, PAGE_EXECUTE_READWRITE, &oldProtection)) {
#else
    if (const auto addressPageAligned = std::uintptr_t(memory->dispatchSound) - std::uintptr_t(memory->dispatchSound) % sysconf(_SC_PAGESIZE);
        mprotect((void*)addressPageAligned, 4, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
#endif
        originalDispatchSound = decltype(originalDispatchSound)(uintptr_t(memory->dispatchSound + 1) + *memory->dispatchSound);
        *memory->dispatchSound = uintptr_t(dispatchSound) - uintptr_t(memory->dispatchSound + 1);
#ifdef _WIN32
        VirtualProtect(memory->dispatchSound, 4, oldProtection, nullptr);
#endif
    }

#ifdef _WIN32
    bspQuery.hookAt(6, listLeavesInBox);
    surface.hookAt(67, lockCursor);

    if constexpr (std::is_same_v<HookType, MinHook>)
        MH_EnableHook(MH_ALL_HOOKS);
#endif
}

#ifdef _WIN32

extern "C" BOOL WINAPI _CRT_INIT(HMODULE moduleHandle, DWORD reason, LPVOID reserved);

static DWORD WINAPI unload(HMODULE moduleHandle) noexcept
{
    Sleep(100);

    interfaces->inputSystem->enableInput(true);
    eventListener->remove();

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    _CRT_INIT(moduleHandle, DLL_PROCESS_DETACH, nullptr);

    FreeLibraryAndExitThread(moduleHandle, 0);
}

#endif

void Hooks::uninstall() noexcept
{
#ifdef _WIN32
    if constexpr (std::is_same_v<HookType, MinHook>) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }
#endif

#ifdef _WIN32
    bspQuery.restore();
#endif
    client.restore();
    clientMode.restore();
    engine.restore();
    modelRender.restore();
    sound.restore();
    surface.restore();
    svCheats.restore();
    viewRender.restore();
    extraHook.restore();
    networkChannel.restore();

    netvars->restore();

    Glow::clearCustomObjects();

#ifdef _WIN32
    SetWindowLongPtrW(window, GWLP_WNDPROC, LONG_PTR(originalWndProc));
    **reinterpret_cast<void***>(memory->present) = originalPresent;
    **reinterpret_cast<void***>(memory->reset) = originalReset;

    if (DWORD oldProtection; VirtualProtect(memory->dispatchSound, 4, PAGE_EXECUTE_READWRITE, &oldProtection)) {
        *memory->dispatchSound = uintptr_t(originalDispatchSound) - uintptr_t(memory->dispatchSound + 1);
        VirtualProtect(memory->dispatchSound, 4, oldProtection, nullptr);
    }

    if (HANDLE thread = CreateThread(nullptr, 0, LPTHREAD_START_ROUTINE(unload), moduleHandle, 0, nullptr))
        CloseHandle(thread);
#else
    *reinterpret_cast<decltype(pollEvent)*>(memory->pollEvent) = pollEvent;
    *reinterpret_cast<decltype(swapWindow)*>(memory->swapWindow) = swapWindow;
#endif
}

#ifndef _WIN32

static int pollEvent(SDL_Event* event) noexcept
{
    [[maybe_unused]] static const auto once = []() noexcept {
        netvars = std::make_unique<Netvars>();
        eventListener = std::make_unique<EventListener>();

        ImGui::CreateContext();
        config = std::make_unique<Config>("Osiris");

        gui = std::make_unique<GUI>();

        hooks->install();

        return true;
    }();

    const auto result = hooks->pollEvent(event);

    if (result && ImGui_ImplSDL2_ProcessEvent(event) && gui->isOpen())
        event->type = 0;

    return result;
}

Hooks::Hooks() noexcept
{
    interfaces = std::make_unique<const Interfaces>();
    memory = std::make_unique<const Memory>();

    pollEvent = *reinterpret_cast<decltype(pollEvent)*>(memory->pollEvent);
    *reinterpret_cast<decltype(::pollEvent)**>(memory->pollEvent) = ::pollEvent;
}

#endif

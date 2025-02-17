#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include "imgui/imgui.h"
#include "Hacks/SkinChanger.h"
#include "ConfigStructs.h"
#include "InputUtil.h"

class Config {
public:
    explicit Config(const char*) noexcept;
    void load(size_t, bool incremental) noexcept;
    void load(const char8_t* name, bool incremental) noexcept;
    void save(size_t) const noexcept;
    void add(const char*) noexcept;
    void remove(size_t) noexcept;
    void rename(size_t, const char*) noexcept;
    void reset() noexcept;
    void listConfigs() noexcept;
    void createConfigDir() const noexcept;
    void openConfigDir() const noexcept;

    constexpr auto& getConfigs() noexcept
    {
        return configs;
    }

    struct Legitbot {
        bool enabled{ false };
        bool aimlock{ false };
        bool silent{ false };
        bool friendlyFire{ false };
        bool visibleOnly{ true };
        bool scopedOnly{ true };
        bool ignoreFlash{ false };
        bool ignoreSmoke{ false };
        bool autoShot{ false };
        bool autoScope{ false };
        float fov{ 0.0f };
        float smooth{ 1.0f };
        int killDelay{ 0 };
        int hitChance{ 0 };
        int multiPoint{ 0 };
        int minDamage{ 1 };
        bool autoStop{ false };
        bool hitGroups[5]{ false };
    };
    std::array<Legitbot, 7> legitbot;
    bool legitbotOnKey{ false };
    KeyBind legitbotKey = KeyBind::NONE;
    int legitbotKeyMode{ 0 };

    struct Ragebot {
        bool enabled{ false };
        bool aimlock{ false };
        bool silent{ false };
        bool friendlyFire{ false };
        bool visibleOnly{ true };
        bool scopedOnly{ true };
        bool ignoreFlash{ false };
        bool ignoreSmoke{ false };
        bool autoShot{ false };
        bool autoScope{ false };
        float fov{ 0.0f };
        int hitChance{ 0 };
        int multiPoint{ 0 };
        int minDamage{ 1 };
        bool autoStop{ false };
        bool hitGroups[5]{ false };
        int priority{ 0 };
    };
    std::array<Ragebot, 7> ragebot;
    bool ragebotOnKey{ false };
    KeyBind ragebotKey = KeyBind::NONE;
    int ragebotKeyMode{ 0 };

    struct Triggerbot {
        bool enabled = false;
        bool friendlyFire = false;
        bool scopedOnly = true;
        bool ignoreFlash = false;
        bool ignoreSmoke = false;
        bool killshot = false;
        int shotDelay = 0;
        int hitChance = 0;
        int minDamage = 1;
        float burstTime = 0.0f;
        bool hitGroups[5]{ false };
    };
    std::array<Triggerbot, 7> triggerbot;
    KeyBind triggerbotHoldKey = KeyBind::NONE;

    struct Chams {
        struct Material : Color4 {
            bool enabled = false;
            bool healthBased = false;
            bool blinking = false;
            bool wireframe = false;
            bool cover = false;
            bool ignorez = false;
            int material = 0;
        };
        std::array<Material, 7> materials;
    };

    std::unordered_map<std::string, Chams> chams;
    KeyBindToggle chamsToggleKey = KeyBind::NONE;
    KeyBind chamsHoldKey = KeyBind::NONE;

    struct StreamProofESP {
        KeyBindToggle toggleKey = KeyBind::NONE;
        KeyBind holdKey = KeyBind::NONE;

        std::unordered_map<std::string, Player> allies;
        std::unordered_map<std::string, Player> enemies;
        std::unordered_map<std::string, Weapon> weapons;
        std::unordered_map<std::string, Projectile> projectiles;
        std::unordered_map<std::string, Shared> lootCrates;
        std::unordered_map<std::string, Shared> otherEntities;
    } streamProofESP;

    struct Font {
        ImFont* tiny;
        ImFont* medium;
        ImFont* big;
    };

    struct Visuals {
        bool disablePostProcessing{ false };
        bool inverseRagdollGravity{ false };
        bool noFog{ false };
        bool no3dSky{ false };
        bool noAimPunch{ false };
        bool noViewPunch{ false };
        bool noHands{ false };
        bool noSleeves{ false };
        bool noWeapons{ false };
        bool noSmoke{ false };
        bool noBlur{ false };
        bool noScopeOverlay{ false };
        bool noGrass{ false };
        bool noShadows{ false };
        bool wireframeSmoke{ false };
        bool zoom{ false };
        KeyBindToggle zoomKey = KeyBind::NONE;
        bool thirdperson{ false };
        KeyBindToggle thirdpersonKey = KeyBind::NONE;
        int thirdpersonDistance{ 0 };
        int viewmodelFov{ 0 };
        int fov{ 0 };
        int farZ{ 0 };
        int flashReduction{ 0 };
        float brightness{ 0.0f };
        int skybox{ 0 };
        ColorToggle3 world;
        ColorToggle3 sky;
        bool deagleSpinner{ false };
        int screenEffect{ 0 };
        int hitEffect{ 0 };
        float hitEffectTime{ 0.6f };
        int hitMarker{ 0 };
        float hitMarkerTime{ 0.6f };
        int playerModelT{ 0 };
        int playerModelCT{ 0 };
        BulletTracers bulletTracers;
        ColorToggle molotovHull{ 1.0f, 0.27f, 0.0f, 0.3f };

        struct ColorCorrection {
            bool enabled = false;
            float blue = 0.0f;
            float red = 0.0f;
            float mono = 0.0f;
            float saturation = 0.0f;
            float ghost = 0.0f;
            float green = 0.0f;
            float yellow = 0.0f;
        } colorCorrection;

        int scopeBlend{ 0 };
    } visuals;

    std::array<item_setting, 36> skinChanger;

    struct Sound {
        int chickenVolume{ 100 };

        struct Player {
            int masterVolume{ 100 };
            int headshotVolume{ 100 };
            int weaponVolume{ 100 };
            int footstepVolume{ 100 };
        };

        std::array<Player, 3> players;
    } sound;

    struct Style {
        int menuStyle{ 0 };
        int menuColors{ 0 };
    } style;

    struct Misc {
        Misc() { clanTagText[0] = '\0'; }

        KeyBind menuKey = KeyBind::INSERT;
        bool resolver{ false };
        FakeLatency fakeLatency;
        bool pureBypass = false;
        bool antiAfkKick{ false };
        AutoStrafer autoStrafer;
        bool bunnyHop{ false };
        bool fastDuck{ false };
        bool moonwalk{ false };
        bool edgejump{ false };
        bool slowwalk{ false };
        bool autoPistol{ false };
        bool autoAccept{ false };
        bool radarHack{ false };
        bool revealRanks{ false };
        bool revealMoney{ false };
        bool revealSuspect{ false };
        bool revealVotes{ false };
        bool disableModelOcclusion{ false };
        bool nameStealer{ false };
        bool disablePanoramablur{ false };
        bool killMessage{ false };
        GrenadePredict grenadePrediction;
        bool unlockInventory{ false };
        bool autoGG{ false };
        bool fixTabletSignal{ false };
        bool fakePrime{ false };
        bool fastPlant{ false };
        bool fastStop{ false };
        bool prepareRevolver{ false };
        bool oppositeHandKnife = false;
        PreserveKillfeed preserveKillfeed;
        KeyBind edgejumpkey = KeyBind::NONE;
        KeyBind slowwalkKey = KeyBind::NONE;
        bool forceCrosshair = false;
        bool recoilCrosshair = false;

        struct SpectatorList {
            bool enabled = false;
            bool noTitleBar = false;
            ImVec2 pos;
            ImVec2 size{ 200.0f, 200.0f };
        };

        SpectatorList spectatorList;
        struct Watermark {
            bool enabled = false;
        };
        Watermark watermark;
        float aspectratio{ 0 };
        std::string killMessageString{ "Gotcha!" };
        ColorToggle3 bombTimer{ 1.0f, 0.55f, 0.0f };
        KeyBind prepareRevolverKey = KeyBind::NONE;
        int hitSound{ 0 };
        int quickHealthshotKey{ 0 };
        int killSound{ 0 };
        std::string customKillSound;
        std::string customHitSound;
        PurchaseList purchaseList;

        struct Reportbot {
            bool enabled = false;
            bool textAbuse = false;
            bool griefing = false;
            bool wallhack = true;
            bool aimbot = true;
            bool other = true;
            int target = 0;
            int delay = 1;
            int rounds = 1;
        } reportbot;

        OffscreenEnemies offscreenEnemies;
        ViewmodelChanger viewmodelChanger;
        bool deathmatchGod = false;
        bool antiAimLines = false;
        bool blockBot = false;
        KeyBind blockBotKey = KeyBind::NONE;
        int forceRelayCluster = 0;
        bool clanTag = false;
        char clanTagText[16];
    } misc;

    void scheduleFontLoad(const std::string& name) noexcept;
    bool loadScheduledFonts() noexcept;
    const auto& getSystemFonts() noexcept { return systemFonts; }
    const auto& getFonts() noexcept { return fonts; }
private:
    std::vector<std::string> scheduledFonts{ "Default" };
    std::vector<std::string> systemFonts{ "Default" };
    std::unordered_map<std::string, Font> fonts;
    std::filesystem::path path;
    std::vector<std::string> configs;
};

inline std::unique_ptr<Config> config;

﻿#include <array>
#include <cwctype>
#include <fstream>
#include <functional>
#include <string>

#ifdef _WIN32
#include <ShlObj.h>
#include <Windows.h>
#endif

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_stdlib.h"

#include "imguiCustom.h"

#include "GUI.h"
#include "Config.h"
#include "Hacks/Misc.h"
#include "Hacks/SkinChanger.h"
#include "Helpers.h"
#include "Hooks.h"
#include "Interfaces.h"
#include "SDK/InputSystem.h"
#include "Hacks/Visuals.h"
#include "Hacks/Glow.h"
#include "Hacks/AntiAim.h"
#include "Hacks/Backtrack.h"

constexpr auto windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
| ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

static ImFont* addFontFromVFONT(const std::string& path, float size, const ImWchar* glyphRanges, bool merge) noexcept
{
    auto file = Helpers::loadBinaryFile(path);
    if (!Helpers::decodeVFONT(file))
        return nullptr;

    ImFontConfig cfg;
    cfg.FontData = file.data();
    cfg.FontDataSize = file.size();
    cfg.FontDataOwnedByAtlas = false;
    cfg.MergeMode = merge;
    cfg.GlyphRanges = glyphRanges;
    cfg.SizePixels = size;

    return ImGui::GetIO().Fonts->AddFont(&cfg);
}

GUI::GUI() noexcept
{
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    style.ScrollbarSize = 9.0f;

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ImFontConfig cfg;
    cfg.SizePixels = 15.0f;

#ifdef _WIN32
    if (PWSTR pathToFonts; SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Fonts, 0, nullptr, &pathToFonts))) {
        const std::filesystem::path path{ pathToFonts };
        CoTaskMemFree(pathToFonts);

        fonts.normal15px = io.Fonts->AddFontFromFileTTF((path / "tahoma.ttf").string().c_str(), 15.0f, &cfg, Helpers::getFontGlyphRanges());
        if (!fonts.normal15px)
            io.Fonts->AddFontDefault(&cfg);

        cfg.MergeMode = true;
        static constexpr ImWchar symbol[]{
            0x2605, 0x2605, // ★
            0
        };
        io.Fonts->AddFontFromFileTTF((path / "seguisym.ttf").string().c_str(), 15.0f, &cfg, symbol);
        cfg.MergeMode = false;
    }
#else
    fonts.normal15px = addFontFromVFONT("csgo/panorama/fonts/notosans-regular.vfont", 15.0f, Helpers::getFontGlyphRanges(), false);
#endif
    if (!fonts.normal15px)
        io.Fonts->AddFontDefault(&cfg);
    addFontFromVFONT("csgo/panorama/fonts/notosanskr-regular.vfont", 15.0f, io.Fonts->GetGlyphRangesKorean(), true);
    addFontFromVFONT("csgo/panorama/fonts/notosanssc-regular.vfont", 15.0f, io.Fonts->GetGlyphRangesChineseFull(), true);
}

void GUI::render() noexcept
{
    if (!config->style.menuStyle) {
        renderMenuBar();
        renderLegitbotWindow();
        renderRagebotWindow();
        AntiAim::drawGUI(false);
        renderTriggerbotWindow();
        Backtrack::drawGUI(false);
        Glow::drawGUI(false);
        renderChamsWindow();
        renderStreamProofESPWindow();
        renderVisualsWindow();
        renderSkinChangerWindow();
        renderSoundWindow();
        renderStyleWindow();
        renderMiscWindow();
        renderConfigWindow();
    } else {
        renderGuiStyle2();
    }
}

void GUI::updateColors() const noexcept
{
    switch (config->style.menuColors) {
    case 0: ImGui::StyleColorsDark(); break;
    case 1: ImGui::StyleColorsLight(); break;
    case 2: ImGui::StyleColorsClassic(); break;
    }
}

#include "InputUtil.h"

static void hotkey2(const char* label, KeyBind& key, float samelineOffset = 0.0f, const ImVec2& size = { 100.0f, 0.0f }) noexcept
{
    const auto id = ImGui::GetID(label);
    ImGui::PushID(label);

    ImGui::TextUnformatted(label);
    ImGui::SameLine(samelineOffset);

    if (ImGui::GetActiveID() == id) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));
        ImGui::Button("...", size);
        ImGui::PopStyleColor();

        ImGui::GetCurrentContext()->ActiveIdAllowOverlap = true;
        if ((!ImGui::IsItemHovered() && ImGui::GetIO().MouseClicked[0]) || key.setToPressedKey())
            ImGui::ClearActiveID();
    } else if (ImGui::Button(key.toString(), size)) {
        ImGui::SetActiveID(id, ImGui::GetCurrentWindow());
    }

    ImGui::PopID();
}

void GUI::handleToggle() noexcept
{
    if (config->misc.menuKey.isPressed()) {
        open = !open;
        if (!open)
            interfaces->inputSystem->resetInputState();
#ifndef _WIN32
        ImGui::GetIO().MouseDrawCursor = gui->open;
#endif
    }
}

static void menuBarItem(const char* name, bool& enabled) noexcept
{
    if (ImGui::MenuItem(name)) {
        enabled = true;
        ImGui::SetWindowFocus(name);
        ImGui::SetWindowPos(name, { 100.0f, 100.0f });
    }
}

void GUI::renderMenuBar() noexcept
{
    if (ImGui::BeginMainMenuBar()) {
        menuBarItem("Legitbot", window.legitbot);
        menuBarItem("Ragebot", window.ragebot);
        AntiAim::menuBarItem();
        menuBarItem("Triggerbot", window.triggerbot);
        Backtrack::menuBarItem();
        Glow::menuBarItem();
        menuBarItem("Chams", window.chams);
        menuBarItem("ESP", window.streamProofESP);
        menuBarItem("Visuals", window.visuals);
        menuBarItem("Skin changer", window.skinChanger);
        menuBarItem("Sound", window.sound);
        menuBarItem("Style", window.style);
        menuBarItem("Misc", window.misc);
        menuBarItem("Config", window.config);
        ImGui::EndMainMenuBar();   
    }
}

void GUI::renderLegitbotWindow(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!window.legitbot)
            return;
        ImGui::SetNextWindowSize({ 600.0f, 0.0f });
        ImGui::Begin("Legitbot", &window.legitbot, windowFlags);
    }
    ImGui::Checkbox("On key", &config->legitbotOnKey);
    ImGui::SameLine();
    ImGui::PushID("Legitbot Key");
    hotkey2("", config->legitbotKey);
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::PushID(2);
    ImGui::PushItemWidth(70.0f);
    ImGui::Combo("", &config->legitbotKeyMode, "Hold\0Toggle\0");
    ImGui::PopItemWidth();
    ImGui::PopID();
    ImGui::Separator();
    static int currentCategory{ 0 };
    ImGui::PushID(3);
    ImGui::PushItemWidth(110.0f);
    ImGui::Combo("", &currentCategory, "General\0Pistol\0Rifle\0AWP\0Scout\0SMG\0Shotgun\0");
    ImGui::PopItemWidth();
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::Checkbox("Enabled", &config->legitbot[currentCategory].enabled);
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 220.0f);
    ImGui::Checkbox("Aimlock", &config->legitbot[currentCategory].aimlock);
    ImGui::Checkbox("Silent", &config->legitbot[currentCategory].silent);
    ImGui::Checkbox("Friendly fire", &config->legitbot[currentCategory].friendlyFire);
    ImGui::Checkbox("Visible only", &config->legitbot[currentCategory].visibleOnly);
    ImGui::Checkbox("Scoped only", &config->legitbot[currentCategory].scopedOnly);
    ImGui::Checkbox("Ignore flash", &config->legitbot[currentCategory].ignoreFlash);
    ImGui::Checkbox("Ignore smoke", &config->legitbot[currentCategory].ignoreSmoke);
    ImGui::Checkbox("Auto shot", &config->legitbot[currentCategory].autoShot);
    ImGui::Checkbox("Auto scope", &config->legitbot[currentCategory].autoScope);
    ImGui::Checkbox("Auto stop", &config->legitbot[currentCategory].autoStop);
    std::vector<const char*> hitGroups = { "Head", "Chest", "Stomach", "Arms", "Legs" };
    ImGui::multiCombo("Hit groups", hitGroups.data(), config->legitbot[currentCategory].hitGroups, hitGroups.size());
    ImGui::NextColumn();
    ImGui::PushItemWidth(240.0f);
    ImGui::SliderFloat("FOV", &config->legitbot[currentCategory].fov, 0.0f, 255.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Smooth", &config->legitbot[currentCategory].smooth, 1.0f, 100.0f, "%.2f");
    ImGui::SliderInt("Kill delay", &config->legitbot[currentCategory].killDelay, 0, 1000, "%d ms");
    ImGui::SliderInt("Multi point", &config->legitbot[currentCategory].multiPoint, 0, 100, "%d");
    ImGui::SliderInt("Hit chance", &config->legitbot[currentCategory].hitChance, 0, 100, "%d%%");
    ImGui::SliderInt("Minimum damage", &config->legitbot[currentCategory].minDamage, 0, 150, "%d");
    ImGui::Columns(1);
    if (!contentOnly)
        ImGui::End();
}

void GUI::renderRagebotWindow(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!window.ragebot)
            return;
        ImGui::SetNextWindowSize({ 600.0f, 0.0f });
        ImGui::Begin("Ragebot", &window.ragebot, windowFlags);
    }
    ImGui::Checkbox("On key", &config->ragebotOnKey);
    ImGui::SameLine();
    ImGui::PushID("Ragebot Key");
    hotkey2("", config->ragebotKey);
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::PushID(2);
    ImGui::PushItemWidth(70.0f);
    ImGui::Combo("", &config->ragebotKeyMode, "Hold\0Toggle\0");
    ImGui::PopItemWidth();
    ImGui::PopID();
    ImGui::Separator();
    static int currentCategory{ 0 };
    ImGui::PushID(3);
    ImGui::PushItemWidth(110.0f);
    ImGui::Combo("", &currentCategory, "General\0Pistol\0Rifle\0AWP\0Scout\0SMG\0Shotgun\0");
    ImGui::PopItemWidth();
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::Checkbox("Enabled", &config->ragebot[currentCategory].enabled);
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 220.0f);
    ImGui::Checkbox("Aimlock", &config->ragebot[currentCategory].aimlock);
    ImGui::Checkbox("Silent", &config->ragebot[currentCategory].silent);
    ImGui::Checkbox("Friendly fire", &config->ragebot[currentCategory].friendlyFire);
    ImGui::Checkbox("Visible only", &config->ragebot[currentCategory].visibleOnly);
    ImGui::Checkbox("Scoped only", &config->ragebot[currentCategory].scopedOnly);
    ImGui::Checkbox("Ignore flash", &config->ragebot[currentCategory].ignoreFlash);
    ImGui::Checkbox("Ignore smoke", &config->ragebot[currentCategory].ignoreSmoke);
    ImGui::Checkbox("Auto shot", &config->ragebot[currentCategory].autoShot);
    ImGui::Checkbox("Auto scope", &config->ragebot[currentCategory].autoScope);
    ImGui::Checkbox("Auto stop", &config->ragebot[currentCategory].autoStop);
    std::vector<const char*> hitGroups = { "Head", "Chest", "Stomach", "Arms", "Legs" };
    ImGui::multiCombo("Hit groups", hitGroups.data(), config->ragebot[currentCategory].hitGroups, hitGroups.size());
    ImGui::NextColumn();
    ImGui::PushItemWidth(240.0f);
    ImGui::SliderFloat("FOV", &config->ragebot[currentCategory].fov, 0.0f, 255.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderInt("Multi point", &config->ragebot[currentCategory].multiPoint, 0, 100, "%d");
    ImGui::SliderInt("Hit chance", &config->ragebot[currentCategory].hitChance, 0, 100, "%d%%");
    ImGui::SliderInt("Minimum damage", &config->legitbot[currentCategory].minDamage, 0, 150, "%d");
    ImGui::Combo("Priority", &config->ragebot[currentCategory].priority, "Health\0Distance\0Fov\0");
    ImGui::Columns(1);
    if (!contentOnly)
        ImGui::End();
}

void GUI::renderTriggerbotWindow(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!window.triggerbot)
            return;
        ImGui::SetNextWindowSize({ 0.0f, 0.0f });
        ImGui::Begin("Triggerbot", &window.triggerbot, windowFlags);
    }
    static int currentCategory{ 0 };
    ImGui::PushID(0);
    ImGui::PushItemWidth(110.0f);
    ImGui::Combo("", &currentCategory, "General\0Pistol\0Rifle\0AWP\0Scout\0SMG\0Shotgun\0");
    ImGui::PopItemWidth();
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::Checkbox("Enabled", &config->triggerbot[currentCategory].enabled);
    ImGui::Separator();
    hotkey2("Hold Key", config->triggerbotHoldKey);
    ImGui::Checkbox("Friendly fire", &config->triggerbot[currentCategory].friendlyFire);
    ImGui::Checkbox("Scoped only", &config->triggerbot[currentCategory].scopedOnly);
    ImGui::Checkbox("Ignore flash", &config->triggerbot[currentCategory].ignoreFlash);
    ImGui::Checkbox("Ignore smoke", &config->triggerbot[currentCategory].ignoreSmoke);
    std::vector<const char*> hitGroups = { "Head", "Chest", "Stomach", "Arms", "Legs" };
    ImGui::multiCombo("Hit groups", hitGroups.data(), config->triggerbot[currentCategory].hitGroups, hitGroups.size());
    ImGui::PushItemWidth(220.0f);
    ImGui::SliderInt("Shot delay", &config->triggerbot[currentCategory].shotDelay, 0, 250, "%d ms");
    ImGui::SliderInt("Hit chance", &config->triggerbot[currentCategory].hitChance, 0, 100, "%d%%");
    ImGui::InputInt("Min damage", &config->triggerbot[currentCategory].minDamage);
    config->triggerbot[currentCategory].minDamage = std::clamp(config->triggerbot[currentCategory].minDamage, 0, 250);
    ImGui::Checkbox("Killshot", &config->triggerbot[currentCategory].killshot);
    ImGui::SliderFloat("Burst Time", &config->triggerbot[currentCategory].burstTime, 0.0f, 0.5f, "%.3f s");

    if (!contentOnly)
        ImGui::End();
}

void GUI::renderChamsWindow(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!window.chams)
            return;
        ImGui::SetNextWindowSize({ 0.0f, 0.0f });
        ImGui::Begin("Chams", &window.chams, windowFlags);
    }

    hotkey2("Toggle Key", config->chamsToggleKey, 80.0f);
    hotkey2("Hold Key", config->chamsHoldKey, 80.0f);
    ImGui::Separator();

    static int currentCategory{ 0 };
    ImGui::PushItemWidth(110.0f);
    ImGui::PushID(0);

    static int material = 1;

    if (ImGui::Combo("", &currentCategory, "Allies\0Enemies\0Planting\0Defusing\0Local player\0Weapons\0Hands\0Backtrack\0Sleeves\0Fake model\0"))
        material = 1;

    ImGui::PopID();

    ImGui::SameLine();

    if (material <= 1)
        ImGuiCustom::arrowButtonDisabled("##left", ImGuiDir_Left);
    else if (ImGui::ArrowButton("##left", ImGuiDir_Left))
        --material;

    ImGui::SameLine();
    ImGui::Text("%d", material);

    constexpr std::array categories{ "Allies", "Enemies", "Planting", "Defusing", "Local player", "Weapons", "Hands", "Backtrack", "Sleeves", "Fake model" };

    ImGui::SameLine();

    if (material >= int(config->chams[categories[currentCategory]].materials.size()))
        ImGuiCustom::arrowButtonDisabled("##right", ImGuiDir_Right);
    else if (ImGui::ArrowButton("##right", ImGuiDir_Right))
        ++material;

    ImGui::SameLine();

    auto& chams{ config->chams[categories[currentCategory]].materials[material - 1] };

    ImGui::Checkbox("Enabled", &chams.enabled);
    ImGui::Separator();
    ImGui::Checkbox("Health based", &chams.healthBased);
    ImGui::Checkbox("Blinking", &chams.blinking);
    ImGui::Combo("Material", &chams.material, "Normal\0Flat\0Animated\0Platinum\0Glass\0Chrome\0Crystal\0Silver\0Gold\0Plastic\0Glow\0Pearlescent\0Metallic\0");
    ImGui::Checkbox("Wireframe", &chams.wireframe);
    ImGui::Checkbox("Cover", &chams.cover);
    ImGui::Checkbox("Ignore-Z", &chams.ignorez);
    ImGuiCustom::colorPicker("Color", chams);

    if (!contentOnly) {
        ImGui::End();
    }
}

void GUI::renderStreamProofESPWindow(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!window.streamProofESP)
            return;
        ImGui::SetNextWindowSize({ 0.0f, 0.0f });
        ImGui::Begin("ESP", &window.streamProofESP, windowFlags);
    }

    hotkey2("Toggle Key", config->streamProofESP.toggleKey, 80.0f);
    hotkey2("Hold Key", config->streamProofESP.holdKey, 80.0f);
    ImGui::Separator();

    static std::size_t currentCategory;
    static auto currentItem = "All";

    constexpr auto getConfigShared = [](std::size_t category, const char* item) noexcept -> Shared& {
        switch (category) {
        case 0: default: return config->streamProofESP.enemies[item];
        case 1: return config->streamProofESP.allies[item];
        case 2: return config->streamProofESP.weapons[item];
        case 3: return config->streamProofESP.projectiles[item];
        case 4: return config->streamProofESP.lootCrates[item];
        case 5: return config->streamProofESP.otherEntities[item];
        }
    };

    constexpr auto getConfigPlayer = [](std::size_t category, const char* item) noexcept -> Player& {
        switch (category) {
        case 0: default: return config->streamProofESP.enemies[item];
        case 1: return config->streamProofESP.allies[item];
        }
    };

    if (ImGui::BeginListBox("##list", { 170.0f, 300.0f })) {
        constexpr std::array categories{ "Enemies", "Allies", "Weapons", "Projectiles", "Loot Crates", "Other Entities" };

        for (std::size_t i = 0; i < categories.size(); ++i) {
            if (ImGui::Selectable(categories[i], currentCategory == i && std::string_view{ currentItem } == "All")) {
                currentCategory = i;
                currentItem = "All";
            }

            if (ImGui::BeginDragDropSource()) {
                switch (i) {
                case 0: case 1: ImGui::SetDragDropPayload("Player", &getConfigPlayer(i, "All"), sizeof(Player), ImGuiCond_Once); break;
                case 2: ImGui::SetDragDropPayload("Weapon", &config->streamProofESP.weapons["All"], sizeof(Weapon), ImGuiCond_Once); break;
                case 3: ImGui::SetDragDropPayload("Projectile", &config->streamProofESP.projectiles["All"], sizeof(Projectile), ImGuiCond_Once); break;
                default: ImGui::SetDragDropPayload("Entity", &getConfigShared(i, "All"), sizeof(Shared), ImGuiCond_Once); break;
                }
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Player")) {
                    const auto& data = *(Player*)payload->Data;

                    switch (i) {
                    case 0: case 1: getConfigPlayer(i, "All") = data; break;
                    case 2: config->streamProofESP.weapons["All"] = data; break;
                    case 3: config->streamProofESP.projectiles["All"] = data; break;
                    default: getConfigShared(i, "All") = data; break;
                    }
                }

                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Weapon")) {
                    const auto& data = *(Weapon*)payload->Data;

                    switch (i) {
                    case 0: case 1: getConfigPlayer(i, "All") = data; break;
                    case 2: config->streamProofESP.weapons["All"] = data; break;
                    case 3: config->streamProofESP.projectiles["All"] = data; break;
                    default: getConfigShared(i, "All") = data; break;
                    }
                }

                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Projectile")) {
                    const auto& data = *(Projectile*)payload->Data;

                    switch (i) {
                    case 0: case 1: getConfigPlayer(i, "All") = data; break;
                    case 2: config->streamProofESP.weapons["All"] = data; break;
                    case 3: config->streamProofESP.projectiles["All"] = data; break;
                    default: getConfigShared(i, "All") = data; break;
                    }
                }

                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Entity")) {
                    const auto& data = *(Shared*)payload->Data;

                    switch (i) {
                    case 0: case 1: getConfigPlayer(i, "All") = data; break;
                    case 2: config->streamProofESP.weapons["All"] = data; break;
                    case 3: config->streamProofESP.projectiles["All"] = data; break;
                    default: getConfigShared(i, "All") = data; break;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::PushID(i);
            ImGui::Indent();

            const auto items = [](std::size_t category) noexcept -> std::vector<const char*> {
                switch (category) {
                case 0:
                case 1: return { "Visible", "Occluded" };
                case 2: return { "Pistols", "SMGs", "Rifles", "Sniper Rifles", "Shotguns", "Machineguns", "Grenades", "Melee", "Other" };
                case 3: return { "Flashbang", "HE Grenade", "Breach Charge", "Bump Mine", "Decoy Grenade", "Molotov", "TA Grenade", "Smoke Grenade", "Snowball" };
                case 4: return { "Pistol Case", "Light Case", "Heavy Case", "Explosive Case", "Tools Case", "Cash Dufflebag" };
                case 5: return { "Defuse Kit", "Chicken", "Planted C4", "Hostage", "Sentry", "Cash", "Ammo Box", "Radar Jammer", "Snowball Pile", "Collectable Coin" };
                default: return { };
                }
            }(i);

            const auto categoryEnabled = getConfigShared(i, "All").enabled;

            for (std::size_t j = 0; j < items.size(); ++j) {
                static bool selectedSubItem;
                if (!categoryEnabled || getConfigShared(i, items[j]).enabled) {
                    if (ImGui::Selectable(items[j], currentCategory == i && !selectedSubItem && std::string_view{ currentItem } == items[j])) {
                        currentCategory = i;
                        currentItem = items[j];
                        selectedSubItem = false;
                    }

                    if (ImGui::BeginDragDropSource()) {
                        switch (i) {
                        case 0: case 1: ImGui::SetDragDropPayload("Player", &getConfigPlayer(i, items[j]), sizeof(Player), ImGuiCond_Once); break;
                        case 2: ImGui::SetDragDropPayload("Weapon", &config->streamProofESP.weapons[items[j]], sizeof(Weapon), ImGuiCond_Once); break;
                        case 3: ImGui::SetDragDropPayload("Projectile", &config->streamProofESP.projectiles[items[j]], sizeof(Projectile), ImGuiCond_Once); break;
                        default: ImGui::SetDragDropPayload("Entity", &getConfigShared(i, items[j]), sizeof(Shared), ImGuiCond_Once); break;
                        }
                        ImGui::EndDragDropSource();
                    }

                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Player")) {
                            const auto& data = *(Player*)payload->Data;

                            switch (i) {
                            case 0: case 1: getConfigPlayer(i, items[j]) = data; break;
                            case 2: config->streamProofESP.weapons[items[j]] = data; break;
                            case 3: config->streamProofESP.projectiles[items[j]] = data; break;
                            default: getConfigShared(i, items[j]) = data; break;
                            }
                        }

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Weapon")) {
                            const auto& data = *(Weapon*)payload->Data;

                            switch (i) {
                            case 0: case 1: getConfigPlayer(i, items[j]) = data; break;
                            case 2: config->streamProofESP.weapons[items[j]] = data; break;
                            case 3: config->streamProofESP.projectiles[items[j]] = data; break;
                            default: getConfigShared(i, items[j]) = data; break;
                            }
                        }

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Projectile")) {
                            const auto& data = *(Projectile*)payload->Data;

                            switch (i) {
                            case 0: case 1: getConfigPlayer(i, items[j]) = data; break;
                            case 2: config->streamProofESP.weapons[items[j]] = data; break;
                            case 3: config->streamProofESP.projectiles[items[j]] = data; break;
                            default: getConfigShared(i, items[j]) = data; break;
                            }
                        }

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Entity")) {
                            const auto& data = *(Shared*)payload->Data;

                            switch (i) {
                            case 0: case 1: getConfigPlayer(i, items[j]) = data; break;
                            case 2: config->streamProofESP.weapons[items[j]] = data; break;
                            case 3: config->streamProofESP.projectiles[items[j]] = data; break;
                            default: getConfigShared(i, items[j]) = data; break;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }

                if (i != 2)
                    continue;

                ImGui::Indent();

                const auto subItems = [](std::size_t item) noexcept -> std::vector<const char*> {
                    switch (item) {
                    case 0: return { "Glock-18", "P2000", "USP-S", "Dual Berettas", "P250", "Tec-9", "Five-SeveN", "CZ75-Auto", "Desert Eagle", "R8 Revolver" };
                    case 1: return { "MAC-10", "MP9", "MP7", "MP5-SD", "UMP-45", "P90", "PP-Bizon" };
                    case 2: return { "Galil AR", "FAMAS", "AK-47", "M4A4", "M4A1-S", "SG 553", "AUG" };
                    case 3: return { "SSG 08", "AWP", "G3SG1", "SCAR-20" };
                    case 4: return { "Nova", "XM1014", "Sawed-Off", "MAG-7" };
                    case 5: return { "M249", "Negev" };
                    case 6: return { "Flashbang", "HE Grenade", "Smoke Grenade", "Molotov", "Decoy Grenade", "Incendiary", "TA Grenade", "Fire Bomb", "Diversion", "Frag Grenade", "Snowball" };
                    case 7: return { "Axe", "Hammer", "Wrench" };
                    case 8: return { "C4", "Healthshot", "Bump Mine", "Zone Repulsor", "Shield" };
                    default: return { };
                    }
                }(j);

                const auto itemEnabled = getConfigShared(i, items[j]).enabled;

                for (const auto subItem : subItems) {
                    auto& subItemConfig = config->streamProofESP.weapons[subItem];
                    if ((categoryEnabled || itemEnabled) && !subItemConfig.enabled)
                        continue;

                    if (ImGui::Selectable(subItem, currentCategory == i && selectedSubItem && std::string_view{ currentItem } == subItem)) {
                        currentCategory = i;
                        currentItem = subItem;
                        selectedSubItem = true;
                    }

                    if (ImGui::BeginDragDropSource()) {
                        ImGui::SetDragDropPayload("Weapon", &subItemConfig, sizeof(Weapon), ImGuiCond_Once);
                        ImGui::EndDragDropSource();
                    }

                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Player")) {
                            const auto& data = *(Player*)payload->Data;
                            subItemConfig = data;
                        }

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Weapon")) {
                            const auto& data = *(Weapon*)payload->Data;
                            subItemConfig = data;
                        }

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Projectile")) {
                            const auto& data = *(Projectile*)payload->Data;
                            subItemConfig = data;
                        }

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Entity")) {
                            const auto& data = *(Shared*)payload->Data;
                            subItemConfig = data;
                        }
                        ImGui::EndDragDropTarget();
                    }
                }

                ImGui::Unindent();
            }
            ImGui::Unindent();
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }

    ImGui::SameLine();

    if (ImGui::BeginChild("##child", { 400.0f, 0.0f })) {
        auto& sharedConfig = getConfigShared(currentCategory, currentItem);

        ImGui::Checkbox("Enabled", &sharedConfig.enabled);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 260.0f);
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::BeginCombo("Font", config->getSystemFonts()[sharedConfig.font.index].c_str())) {
            for (size_t i = 0; i < config->getSystemFonts().size(); i++) {
                bool isSelected = config->getSystemFonts()[i] == sharedConfig.font.name;
                if (ImGui::Selectable(config->getSystemFonts()[i].c_str(), isSelected, 0, { 250.0f, 0.0f })) {
                    sharedConfig.font.index = i;
                    sharedConfig.font.name = config->getSystemFonts()[i];
                    config->scheduleFontLoad(sharedConfig.font.name);
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();

        constexpr auto spacing = 250.0f;
        ImGuiCustom::colorPicker("Snapline", sharedConfig.snapline);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::Combo("##1", &sharedConfig.snapline.type, "Bottom\0Top\0Crosshair\0");
        ImGui::SameLine(spacing);
        ImGuiCustom::colorPicker("Box", sharedConfig.box);
        ImGui::SameLine();

        ImGui::PushID("Box");

        if (ImGui::Button("..."))
            ImGui::OpenPopup("");

        if (ImGui::BeginPopup("")) {
            ImGui::SetNextItemWidth(95.0f);
            ImGui::Combo("Type", &sharedConfig.box.type, "2D\0" "2D corners\0" "3D\0" "3D corners\0");
            ImGui::SetNextItemWidth(275.0f);
            ImGui::SliderFloat3("Scale", sharedConfig.box.scale.data(), 0.0f, 0.50f, "%.2f");
            ImGuiCustom::colorPicker("Fill", sharedConfig.box.fill);
            ImGui::EndPopup();
        }

        ImGui::PopID();

        ImGuiCustom::colorPicker("Name", sharedConfig.name);
        ImGui::SameLine(spacing);

        if (currentCategory < 2) {
            auto& playerConfig = getConfigPlayer(currentCategory, currentItem);

            ImGuiCustom::colorPicker("Weapon", playerConfig.weapon);
            ImGuiCustom::colorPicker("Flash Duration", playerConfig.flashDuration);
            ImGui::SameLine(spacing);
            ImGuiCustom::colorPicker("Skeleton", playerConfig.skeleton);
            ImGui::Checkbox("Audible Only", &playerConfig.audibleOnly);
            ImGui::SameLine(spacing);
            ImGui::Checkbox("Spotted Only", &playerConfig.spottedOnly);

            ImGuiCustom::colorPicker("Head Box", playerConfig.headBox);
            ImGui::SameLine();

            ImGui::PushID("Head Box");

            if (ImGui::Button("..."))
                ImGui::OpenPopup("");

            if (ImGui::BeginPopup("")) {
                ImGui::SetNextItemWidth(95.0f);
                ImGui::Combo("Type", &playerConfig.headBox.type, "2D\0" "2D corners\0" "3D\0" "3D corners\0");
                ImGui::SetNextItemWidth(275.0f);
                ImGui::SliderFloat3("Scale", playerConfig.headBox.scale.data(), 0.0f, 0.50f, "%.2f");
                ImGuiCustom::colorPicker("Fill", playerConfig.headBox.fill);
                ImGui::EndPopup();
            }

            ImGui::PopID();
        
            ImGui::SameLine(spacing);
            ImGui::Checkbox("Health Bar", &playerConfig.healthBar);
        } else if (currentCategory == 2) {
            auto& weaponConfig = config->streamProofESP.weapons[currentItem];
            ImGuiCustom::colorPicker("Ammo", weaponConfig.ammo);
        } else if (currentCategory == 3) {
            auto& trails = config->streamProofESP.projectiles[currentItem].trails;

            ImGui::Checkbox("Trails", &trails.enabled);
            ImGui::SameLine(spacing + 77.0f);
            ImGui::PushID("Trails");

            if (ImGui::Button("..."))
                ImGui::OpenPopup("");

            if (ImGui::BeginPopup("")) {
                constexpr auto trailPicker = [](const char* name, Trail& trail) noexcept {
                    ImGui::PushID(name);
                    ImGuiCustom::colorPicker(name, trail);
                    ImGui::SameLine(150.0f);
                    ImGui::SetNextItemWidth(95.0f);
                    ImGui::Combo("", &trail.type, "Line\0Circles\0Filled Circles\0");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(95.0f);
                    ImGui::InputFloat("Time", &trail.time, 0.1f, 0.5f, "%.1fs");
                    trail.time = std::clamp(trail.time, 1.0f, 60.0f);
                    ImGui::PopID();
                };

                trailPicker("Local Player", trails.localPlayer);
                trailPicker("Allies", trails.allies);
                trailPicker("Enemies", trails.enemies);
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        ImGui::SetNextItemWidth(95.0f);
        ImGui::InputFloat("Text Cull Distance", &sharedConfig.textCullDistance, 0.4f, 0.8f, "%.1fm");
        sharedConfig.textCullDistance = std::clamp(sharedConfig.textCullDistance, 0.0f, 999.9f);
    }

    ImGui::EndChild();

    if (!contentOnly)
        ImGui::End();
}

void GUI::renderVisualsWindow(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!window.visuals)
            return;
        ImGui::SetNextWindowSize({ 680.0f, 0.0f });
        ImGui::Begin("Visuals", &window.visuals, windowFlags);
    }
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 280.0f);
    constexpr auto playerModels = "Default\0Special Agent Ava | FBI\0Operator | FBI SWAT\0Markus Delrow | FBI HRT\0Michael Syfers | FBI Sniper\0B Squadron Officer | SAS\0Seal Team 6 Soldier | NSWC SEAL\0Buckshot | NSWC SEAL\0Lt. Commander Ricksaw | NSWC SEAL\0Third Commando Company | KSK\0'Two Times' McCoy | USAF TACP\0Dragomir | Sabre\0Rezan The Ready | Sabre\0'The Doctor' Romanov | Sabre\0Maximus | Sabre\0Blackwolf | Sabre\0The Elite Mr. Muhlik | Elite Crew\0Ground Rebel | Elite Crew\0Osiris | Elite Crew\0Prof. Shahmat | Elite Crew\0Enforcer | Phoenix\0Slingshot | Phoenix\0Soldier | Phoenix\0Pirate\0Pirate Variant A\0Pirate Variant B\0Pirate Variant C\0Pirate Variant D\0Anarchist\0Anarchist Variant A\0Anarchist Variant B\0Anarchist Variant C\0Anarchist Variant D\0Balkan Variant A\0Balkan Variant B\0Balkan Variant C\0Balkan Variant D\0Balkan Variant E\0Jumpsuit Variant A\0Jumpsuit Variant B\0Jumpsuit Variant C\0Street Soldier | Phoenix\0'Blueberries' Buckshot | NSWC SEAL\0'Two Times' McCoy | TACP Cavalry\0Rezan the Redshirt | Sabre\0Dragomir | Sabre Footsoldier\0Cmdr. Mae 'Dead Cold' Jamison | SWAT\0001st Lieutenant Farlow | SWAT\0John 'Van Healen' Kask | SWAT\0Bio-Haz Specialist | SWAT\0Sergeant Bombson | SWAT\0Chem-Haz Specialist | SWAT\0Sir Bloody Miami Darryl | The Professionals\0Sir Bloody Silent Darryl | The Professionals\0Sir Bloody Skullhead Darryl | The Professionals\0Sir Bloody Darryl Royale | The Professionals\0Sir Bloody Loudmouth Darryl | The Professionals\0Safecracker Voltzmann | The Professionals\0Little Kev | The Professionals\0Number K | The Professionals\0Getaway Sally | The Professionals\0";
    if (ImGui::Combo("T Player Model", &config->visuals.playerModelT, playerModels))
        SkinChanger::scheduleHudUpdate();
    if (ImGui::Combo("CT Player Model", &config->visuals.playerModelCT, playerModels))
        SkinChanger::scheduleHudUpdate();
    ImGui::Checkbox("Disable post-processing", &config->visuals.disablePostProcessing);
    ImGui::Checkbox("Inverse ragdoll gravity", &config->visuals.inverseRagdollGravity);
    ImGui::Checkbox("No fog", &config->visuals.noFog);
    ImGui::Checkbox("No 3d sky", &config->visuals.no3dSky);
    ImGui::Checkbox("No aim punch", &config->visuals.noAimPunch);
    ImGui::Checkbox("No view punch", &config->visuals.noViewPunch);
    ImGui::Checkbox("No hands", &config->visuals.noHands);
    ImGui::Checkbox("No sleeves", &config->visuals.noSleeves);
    ImGui::Checkbox("No weapons", &config->visuals.noWeapons);
    ImGui::Checkbox("No smoke", &config->visuals.noSmoke);
    ImGui::Checkbox("No blur", &config->visuals.noBlur);
    ImGui::Checkbox("No scope overlay", &config->visuals.noScopeOverlay);
    ImGui::Checkbox("No grass", &config->visuals.noGrass);
    ImGui::Checkbox("No shadows", &config->visuals.noShadows);
    ImGui::Checkbox("Wireframe smoke", &config->visuals.wireframeSmoke);
    ImGui::NextColumn();
    ImGui::Checkbox("Zoom", &config->visuals.zoom);
    ImGui::SameLine();
    ImGui::PushID("Zoom Key");
    hotkey2("", config->visuals.zoomKey);
    ImGui::PopID();
    ImGui::Checkbox("Thirdperson", &config->visuals.thirdperson);
    ImGui::SameLine();
    ImGui::PushID("Thirdperson Key");
    hotkey2("", config->visuals.thirdpersonKey);
    ImGui::PopID();
    ImGui::PushItemWidth(290.0f);
    ImGui::PushID(0);
    ImGui::SliderInt("", &config->visuals.thirdpersonDistance, 0, 1000, "Thirdperson distance: %d");
    ImGui::PopID();
    ImGui::PushID(1);
    ImGui::SliderInt("", &config->visuals.scopeBlend, 0, 100, "Scope blend: %d%%");
    ImGui::PopID();
    ImGui::PushID(2);
    ImGui::SliderInt("", &config->visuals.viewmodelFov, -60, 60, "Viewmodel FOV: %d");
    ImGui::PopID();
    ImGui::PushID(3);
    ImGui::SliderInt("", &config->visuals.fov, -60, 60, "FOV: %d");
    ImGui::PopID();
    ImGui::PushID(4);
    ImGui::SliderInt("", &config->visuals.farZ, 0, 2000, "Far Z: %d");
    ImGui::PopID();
    ImGui::PushID(5);
    ImGui::SliderInt("", &config->visuals.flashReduction, 0, 100, "Flash reduction: %d%%");
    ImGui::PopID();
    ImGui::PushID(6);
    ImGui::SliderFloat("", &config->visuals.brightness, 0.0f, 1.0f, "Brightness: %.2f");
    ImGui::PopID();
    ImGui::PopItemWidth();
    ImGui::Combo("Skybox", &config->visuals.skybox, Visuals::skyboxList.data(), Visuals::skyboxList.size());
    ImGuiCustom::colorPicker("World color", config->visuals.world);
    ImGuiCustom::colorPicker("Sky color", config->visuals.sky);
    ImGui::Checkbox("Deagle spinner", &config->visuals.deagleSpinner);
    ImGui::Combo("Screen effect", &config->visuals.screenEffect, "None\0Drone cam\0Drone cam with noise\0Underwater\0Healthboost\0Dangerzone\0");
    ImGui::Combo("Hit effect", &config->visuals.hitEffect, "None\0Drone cam\0Drone cam with noise\0Underwater\0Healthboost\0Dangerzone\0");
    ImGui::SliderFloat("Hit effect time", &config->visuals.hitEffectTime, 0.1f, 1.5f, "%.2fs");
    ImGui::Combo("Hit marker", &config->visuals.hitMarker, "None\0Default (Cross)\0");
    ImGui::SliderFloat("Hit marker time", &config->visuals.hitMarkerTime, 0.1f, 1.5f, "%.2fs");
    ImGuiCustom::colorPicker("Bullet Tracers", config->visuals.bulletTracers.color.color.data(), &config->visuals.bulletTracers.color.color[3], nullptr, nullptr, &config->visuals.bulletTracers.enabled);
    ImGuiCustom::colorPicker("Molotov Hull", config->visuals.molotovHull);

    ImGui::Checkbox("Color correction", &config->visuals.colorCorrection.enabled);
    ImGui::SameLine();
    bool ccPopup = ImGui::Button("Edit");

    if (ccPopup)
        ImGui::OpenPopup("##popup");

    if (ImGui::BeginPopup("##popup")) {
        ImGui::VSliderFloat("##1", { 40.0f, 160.0f }, &config->visuals.colorCorrection.blue, 0.0f, 1.0f, "Blue\n%.3f"); ImGui::SameLine();
        ImGui::VSliderFloat("##2", { 40.0f, 160.0f }, &config->visuals.colorCorrection.red, 0.0f, 1.0f, "Red\n%.3f"); ImGui::SameLine();
        ImGui::VSliderFloat("##3", { 40.0f, 160.0f }, &config->visuals.colorCorrection.mono, 0.0f, 1.0f, "Mono\n%.3f"); ImGui::SameLine();
        ImGui::VSliderFloat("##4", { 40.0f, 160.0f }, &config->visuals.colorCorrection.saturation, 0.0f, 1.0f, "Sat\n%.3f"); ImGui::SameLine();
        ImGui::VSliderFloat("##5", { 40.0f, 160.0f }, &config->visuals.colorCorrection.ghost, 0.0f, 1.0f, "Ghost\n%.3f"); ImGui::SameLine();
        ImGui::VSliderFloat("##6", { 40.0f, 160.0f }, &config->visuals.colorCorrection.green, 0.0f, 1.0f, "Green\n%.3f"); ImGui::SameLine();
        ImGui::VSliderFloat("##7", { 40.0f, 160.0f }, &config->visuals.colorCorrection.yellow, 0.0f, 1.0f, "Yellow\n%.3f"); ImGui::SameLine();
        ImGui::EndPopup();
    }
    ImGui::Columns(1);

    if (!contentOnly)
        ImGui::End();
}

void GUI::renderSkinChangerWindow(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!window.skinChanger)
            return;
        ImGui::SetNextWindowSize({ 700.0f, 0.0f });
        if (!ImGui::Begin("Skin changer", &window.skinChanger, windowFlags)) {
            ImGui::End();
            return;
        }
    }

    static auto itemIndex = 0;

    ImGui::PushItemWidth(110.0f);
    ImGui::Combo("##1", &itemIndex, [](void* data, int idx, const char** out_text) {
        *out_text = SkinChanger::weapon_names[idx].name;
        return true;
        }, nullptr, SkinChanger::weapon_names.size(), 5);
    ImGui::PopItemWidth();

    auto& selected_entry = config->skinChanger[itemIndex];
    selected_entry.itemIdIndex = itemIndex;

    constexpr auto rarityColor = [](int rarity) {
        constexpr auto rarityColors = std::to_array<ImU32>({
            IM_COL32(0,     0,   0,   0),
            IM_COL32(176, 195, 217, 255),
            IM_COL32( 94, 152, 217, 255),
            IM_COL32( 75, 105, 255, 255),
            IM_COL32(136,  71, 255, 255),
            IM_COL32(211,  44, 230, 255),
            IM_COL32(235,  75,  75, 255),
            IM_COL32(228, 174,  57, 255)
        });
        return rarityColors[static_cast<std::size_t>(rarity) < rarityColors.size() ? rarity : 0];
    };

    constexpr auto passesFilter = [](const std::wstring& str, std::wstring filter) {
        constexpr auto delimiter = L" ";
        wchar_t* _;
        wchar_t* token = std::wcstok(filter.data(), delimiter, &_);
        while (token) {
            if (!std::wcsstr(str.c_str(), token))
                return false;
            token = std::wcstok(nullptr, delimiter, &_);
        }
        return true;
    };

    {
        ImGui::SameLine();
        ImGui::Checkbox("Enabled", &selected_entry.enabled);
        ImGui::Separator();
        ImGui::Columns(2, nullptr, false);
        ImGui::InputInt("Seed", &selected_entry.seed);
        ImGui::InputInt("StatTrak\u2122", &selected_entry.stat_trak);
        selected_entry.stat_trak = (std::max)(selected_entry.stat_trak, -1);
        ImGui::SliderFloat("Wear", &selected_entry.wear, FLT_MIN, 1.f, "%.10f", ImGuiSliderFlags_Logarithmic);

        const auto& kits = itemIndex == 1 ? SkinChanger::getGloveKits() : SkinChanger::getSkinKits();

        if (ImGui::BeginCombo("Paint Kit", kits[selected_entry.paint_kit_vector_index].name.c_str())) {
            ImGui::PushID("Paint Kit");
            ImGui::PushID("Search");
            ImGui::SetNextItemWidth(-1.0f);
            static std::array<std::string, SkinChanger::weapon_names.size()> filters;
            auto& filter = filters[itemIndex];
            ImGui::InputTextWithHint("", "Search", &filter);
            if (ImGui::IsItemHovered() || (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)))
                ImGui::SetKeyboardFocusHere(-1);
            ImGui::PopID();

            const std::wstring filterWide = Helpers::toUpper(Helpers::toWideString(filter));
            if (ImGui::BeginChild("##scrollarea", { 0, 6 * ImGui::GetTextLineHeightWithSpacing() })) {
                for (std::size_t i = 0; i < kits.size(); ++i) {
                    if (filter.empty() || passesFilter(kits[i].nameUpperCase, filterWide)) {
                        ImGui::PushID(i);
                        const auto selected = i == selected_entry.paint_kit_vector_index;
                        if (ImGui::SelectableWithBullet(kits[i].name.c_str(), rarityColor(kits[i].rarity), selected)) {
                            selected_entry.paint_kit_vector_index = i;
                            ImGui::CloseCurrentPopup();
                        }

                        if (ImGui::IsItemHovered()) {
                            if (const auto icon = SkinChanger::getItemIconTexture(kits[i].iconPath)) {
                                ImGui::BeginTooltip();
                                ImGui::Image(icon, { 200.0f, 150.0f });
                                ImGui::EndTooltip();
                            }
                        }
                        if (selected && ImGui::IsWindowAppearing())
                            ImGui::SetScrollHereY();
                        ImGui::PopID();
                    }
                }
                ImGui::EndChild();
            }
            ImGui::PopID();
            ImGui::EndCombo();
        }

        ImGui::Combo("Quality", &selected_entry.entity_quality_vector_index, [](void* data, int idx, const char** out_text) {
            *out_text = SkinChanger::getQualities()[idx].name.c_str(); // safe within this lamba
            return true;
            }, nullptr, SkinChanger::getQualities().size(), 5);

        if (itemIndex == 0) {
            ImGui::Combo("Knife", &selected_entry.definition_override_vector_index, [](void* data, int idx, const char** out_text) {
                *out_text = SkinChanger::getKnifeTypes()[idx].name.c_str();
                return true;
                }, nullptr, SkinChanger::getKnifeTypes().size(), 5);
        } else if (itemIndex == 1) {
            ImGui::Combo("Glove", &selected_entry.definition_override_vector_index, [](void* data, int idx, const char** out_text) {
                *out_text = SkinChanger::getGloveTypes()[idx].name.c_str();
                return true;
                }, nullptr, SkinChanger::getGloveTypes().size(), 5);
        } else {
            static auto unused_value = 0;
            selected_entry.definition_override_vector_index = 0;
            ImGui::Combo("Unavailable", &unused_value, "For knives or gloves\0");
        }

        ImGui::InputText("Name Tag", selected_entry.custom_name, 32);
    }

    ImGui::NextColumn();

    {
        ImGui::PushID("sticker");

        static std::size_t selectedStickerSlot = 0;

        ImGui::PushItemWidth(-1);
        ImVec2 size;
        size.x = 0.0f;
        size.y = ImGui::GetTextLineHeightWithSpacing() * 5.25f + ImGui::GetStyle().FramePadding.y * 2.0f;

        if (ImGui::BeginListBox("", size)) {
            for (int i = 0; i < 5; ++i) {
                ImGui::PushID(i);

                const auto kit_vector_index = config->skinChanger[itemIndex].stickers[i].kit_vector_index;
                const std::string text = '#' + std::to_string(i + 1) + "  " + SkinChanger::getStickerKits()[kit_vector_index].name;

                if (ImGui::Selectable(text.c_str(), i == selectedStickerSlot))
                    selectedStickerSlot = i;

                ImGui::PopID();
            }
            ImGui::EndListBox();
        }

        ImGui::PopItemWidth();

        auto& selected_sticker = selected_entry.stickers[selectedStickerSlot];

        const auto& kits = SkinChanger::getStickerKits();
        if (ImGui::BeginCombo("Sticker", kits[selected_sticker.kit_vector_index].name.c_str())) {
            ImGui::PushID("Sticker");
            ImGui::PushID("Search");
            ImGui::SetNextItemWidth(-1.0f);
            static std::array<std::string, SkinChanger::weapon_names.size()> filters;
            auto& filter = filters[itemIndex];
            ImGui::InputTextWithHint("", "Search", &filter);
            if (ImGui::IsItemHovered() || (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)))
                ImGui::SetKeyboardFocusHere(-1);
            ImGui::PopID();

            const std::wstring filterWide = Helpers::toUpper(Helpers::toWideString(filter));
            if (ImGui::BeginChild("##scrollarea", { 0, 6 * ImGui::GetTextLineHeightWithSpacing() })) {
                for (std::size_t i = 0; i < kits.size(); ++i) {
                    if (filter.empty() || passesFilter(kits[i].nameUpperCase, filterWide)) {
                        ImGui::PushID(i);
                        const auto selected = i == selected_sticker.kit_vector_index;
                        if (ImGui::SelectableWithBullet(kits[i].name.c_str(), rarityColor(kits[i].rarity), selected)) {
                            selected_sticker.kit_vector_index = i;
                            ImGui::CloseCurrentPopup();
                        }
                        if (ImGui::IsItemHovered()) {
                            if (const auto icon = SkinChanger::getItemIconTexture(kits[i].iconPath)) {
                                ImGui::BeginTooltip();
                                ImGui::Image(icon, { 200.0f, 150.0f });
                                ImGui::EndTooltip();
                            }
                        }
                        if (selected && ImGui::IsWindowAppearing())
                            ImGui::SetScrollHereY();
                        ImGui::PopID();
                    }
                }
                ImGui::EndChild();
            }
            ImGui::PopID();
            ImGui::EndCombo();
        }

        ImGui::SliderFloat("Wear", &selected_sticker.wear, FLT_MIN, 1.0f, "%.10f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Scale", &selected_sticker.scale, 0.1f, 5.0f);
        ImGui::SliderFloat("Rotation", &selected_sticker.rotation, 0.0f, 360.0f);

        ImGui::PopID();
    }
    selected_entry.update();

    ImGui::Columns(1);

    ImGui::Separator();

    if (ImGui::Button("Update", { 130.0f, 30.0f }))
        SkinChanger::scheduleHudUpdate();

    ImGui::TextUnformatted("nSkinz by namazso");

    if (!contentOnly)
        ImGui::End();
}

void GUI::renderSoundWindow(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!window.sound)
            return;
        ImGui::SetNextWindowSize({ 0.0f, 0.0f });
        ImGui::Begin("Sound", &window.sound, windowFlags);
    }
    ImGui::SliderInt("Chicken volume", &config->sound.chickenVolume, 0, 200, "%d%%");

    static int currentCategory{ 0 };
    ImGui::PushItemWidth(110.0f);
    ImGui::Combo("", &currentCategory, "Local player\0Allies\0Enemies\0");
    ImGui::PopItemWidth();
    ImGui::SliderInt("Master volume", &config->sound.players[currentCategory].masterVolume, 0, 200, "%d%%");
    ImGui::SliderInt("Headshot volume", &config->sound.players[currentCategory].headshotVolume, 0, 200, "%d%%");
    ImGui::SliderInt("Weapon volume", &config->sound.players[currentCategory].weaponVolume, 0, 200, "%d%%");
    ImGui::SliderInt("Footstep volume", &config->sound.players[currentCategory].footstepVolume, 0, 200, "%d%%");

    if (!contentOnly)
        ImGui::End();
}

void GUI::renderStyleWindow(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!window.style)
            return;
        ImGui::SetNextWindowSize({ 0.0f, 0.0f });
        ImGui::Begin("Style", &window.style, windowFlags);
    }

    ImGui::PushItemWidth(150.0f);
    if (ImGui::Combo("Menu style", &config->style.menuStyle, "Classic\0One window\0"))
        window = { };
    if (ImGui::Combo("Menu colors", &config->style.menuColors, "Dark\0Light\0Classic\0Custom\0"))
        updateColors();
    ImGui::PopItemWidth();

    if (config->style.menuColors == 3) {
        ImGuiStyle& style = ImGui::GetStyle();
        for (int i = 0; i < ImGuiCol_COUNT; i++) {
            if (i && i & 3) ImGui::SameLine(220.0f * (i & 3));

            ImGuiCustom::colorPicker(ImGui::GetStyleColorName(i), (float*)&style.Colors[i], &style.Colors[i].w);
        }
    }

    if (!contentOnly)
        ImGui::End();
}

void GUI::renderMiscWindow(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!window.misc)
            return;
        ImGui::SetNextWindowSize({ 580.0f, 0.0f });
        ImGui::Begin("Misc", &window.misc, windowFlags);
    }
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 230.0f);
    hotkey2("Menu Key", config->misc.menuKey);
    ImGui::Checkbox("Resolver", &config->misc.resolver);
    ImGui::Checkbox("Fake latency", &config->misc.fakeLatency.enabled);
    ImGui::PushID("Fake latency amount");
    ImGui::SliderInt("Amount", &config->misc.fakeLatency.amount, 1, 200, "%d ms");
    ImGui::PopID();
    ImGui::Checkbox("Bypass sv_pure", &config->misc.pureBypass);
    ImGui::PushItemWidth(120.0f);
    ImGui::Combo("Force relay cluster", &config->misc.forceRelayCluster, "Off\0Australia\0Austria\0Brazil\0Chile\0Dubai\0France\0Germany\0Hong Kong\0India (Chennai)\0India (Mumbai)\0Japan\0Luxembourg\0Netherlands\0Peru\0Philipines\0Poland\0Singapore\0South Africa\0Spain\0Sweden\0UK\0USA (Atlanta)\0USA (Seattle)\0USA (Chicago)\0USA (Los Angeles)\0USA (Moses Lake)\0USA (Oklahoma)\0USA (Seattle)\0USA (Washington DC)\0");
    ImGui::PopItemWidth();
    ImGui::Checkbox("Anti AFK kick", &config->misc.antiAfkKick);
    ImGui::Checkbox("Auto strafer", &config->misc.autoStrafer.enabled);
    ImGui::PushID("Auto strafer mode");
    ImGui::Combo("Mode", &config->misc.autoStrafer.mode, "Legit\0Rage\0");
    ImGui::PopID();
    ImGui::Checkbox("Bunny hop", &config->misc.bunnyHop);
    ImGui::Checkbox("Fast duck", &config->misc.fastDuck);
    ImGui::Checkbox("Moonwalk", &config->misc.moonwalk);
    ImGui::Checkbox("Block bot", &config->misc.blockBot);
    ImGui::SameLine();
    ImGui::PushID("Block bot key");
    hotkey2("", config->misc.blockBotKey);
    ImGui::PopID();
    ImGui::Checkbox("Edge Jump", &config->misc.edgejump);
    ImGui::SameLine();
    ImGui::PushID("Edge Jump Key");
    hotkey2("", config->misc.edgejumpkey);
    ImGui::PopID();
    ImGui::Checkbox("Slowwalk", &config->misc.slowwalk);
    ImGui::SameLine();
    ImGui::PushID("Slowwalk Key");
    hotkey2("", config->misc.slowwalkKey);
    ImGui::PopID();
    ImGui::Checkbox("Force crosshair", &config->misc.forceCrosshair);
    ImGui::Checkbox("Recoil crosshair", &config->misc.recoilCrosshair);
    ImGui::Checkbox("Auto pistol", &config->misc.autoPistol);
    ImGui::Checkbox("Auto accept", &config->misc.autoAccept);
    ImGui::Checkbox("Radar hack", &config->misc.radarHack);
    ImGui::Checkbox("Reveal ranks", &config->misc.revealRanks);
    ImGui::Checkbox("Reveal money", &config->misc.revealMoney);
    ImGui::Checkbox("Reveal suspect", &config->misc.revealSuspect);
    ImGui::Checkbox("Reveal votes", &config->misc.revealVotes);

    ImGui::Checkbox("Spectator list", &config->misc.spectatorList.enabled);
    ImGui::SameLine();

    ImGui::PushID("Spectator list");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::Checkbox("No Title Bar", &config->misc.spectatorList.noTitleBar);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::Checkbox("Watermark", &config->misc.watermark.enabled);
    ImGuiCustom::colorPicker("Offscreen Enemies", config->misc.offscreenEnemies.color, &config->misc.offscreenEnemies.enabled);
    ImGui::SameLine();

    ImGui::PushID("Offscreen Enemies");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::SliderInt("Distance", &config->misc.offscreenEnemies.distance, 10, 100, "%d%%");
        ImGui::EndPopup();
    }
    ImGui::PopID();
    ImGui::Checkbox("Disable model occlusion", &config->misc.disableModelOcclusion);
    ImGui::SliderFloat("Aspect Ratio", &config->misc.aspectratio, 0.0f, 5.0f, "%.2f");
    ImGui::NextColumn();
    ImGui::Checkbox("Disable HUD blur", &config->misc.disablePanoramablur);
    ImGui::Checkbox("Clantag", &config->misc.clanTag);
    ImGui::SameLine();
    ImGui::PushItemWidth(120.0f);
    ImGui::PushID(0);
    if (ImGui::InputText("", config->misc.clanTagText, sizeof(config->misc.clanTagText)))
        Misc::clanTag(true);
    ImGui::PopID();
    ImGui::Checkbox("Kill message", &config->misc.killMessage);
    ImGui::SameLine();
    ImGui::PushItemWidth(120.0f);
    ImGui::PushID(1);
    ImGui::InputText("", &config->misc.killMessageString);
    ImGui::PopID();
    ImGui::Checkbox("Name stealer", &config->misc.nameStealer);
    ImGui::PushID(3);
    ImGui::Checkbox("Fast plant", &config->misc.fastPlant);
    ImGui::Checkbox("Fast Stop", &config->misc.fastStop);
    ImGuiCustom::colorPicker("Bomb timer", config->misc.bombTimer);
    ImGui::Checkbox("Prepare revolver", &config->misc.prepareRevolver);
    ImGui::SameLine();
    ImGui::PushID("Prepare revolver Key");
    hotkey2("", config->misc.prepareRevolverKey);
    ImGui::PopID();
    ImGui::Combo("Hit Sound", &config->misc.hitSound, "None\0Metal\0Gamesense\0Bell\0Glass\0Custom\0");
    if (config->misc.hitSound == 5) {
        ImGui::InputText("Hit Sound filename", &config->misc.customHitSound);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("audio file must be put in csgo/sound/ directory");
    }
    ImGui::PushID(5);
    ImGui::Combo("Kill Sound", &config->misc.killSound, "None\0Metal\0Gamesense\0Bell\0Glass\0Custom\0");
    if (config->misc.killSound == 5) {
        ImGui::InputText("Kill Sound filename", &config->misc.customKillSound);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("audio file must be put in csgo/sound/ directory");
    }
    ImGui::Checkbox("Grenade prediction", &config->misc.grenadePrediction.enabled);
    ImGui::SameLine();

    ImGui::PushID("Grenade prediction");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGuiCustom::colorPicker("Line color", config->misc.grenadePrediction.color.color.data(), &config->misc.grenadePrediction.color.color[3]);
        ImGuiCustom::colorPicker("Bounce point color", config->misc.grenadePrediction.bounceColor.color.data(), &config->misc.grenadePrediction.bounceColor.color[3]);
        ImGui::EndPopup();
    }
    ImGui::PopID();
    ImGui::Checkbox("Unlock inventory", &config->misc.unlockInventory);
    ImGui::Checkbox("Auto GG", &config->misc.autoGG);
    ImGui::Checkbox("Fix tablet signal", &config->misc.fixTabletSignal);
    ImGui::Checkbox("Fake prime", &config->misc.fakePrime);
    ImGui::Checkbox("Deathmatch godmode", &config->misc.deathmatchGod);
    ImGui::Checkbox("Opposite Hand Knife", &config->misc.oppositeHandKnife);
    ImGui::Checkbox("Anti aim lines", &config->misc.antiAimLines);
    ImGui::Checkbox("Preserve Killfeed", &config->misc.preserveKillfeed.enabled);
    ImGui::SameLine();

    ImGui::PushID("Preserve Killfeed");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::Checkbox("Only Headshots", &config->misc.preserveKillfeed.onlyHeadshots);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::Checkbox("Viewmodel changer", &config->misc.viewmodelChanger.enabled);
    ImGui::SameLine();

    ImGui::PushID("Viewmodel changer");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::PushItemWidth(290.0f);
        ImGui::PushID("Viewmodel offset X");
        ImGui::SliderFloat("", &config->misc.viewmodelChanger.x, -10.f, 10.f, "Viewmodel offset X: %.2f");
        ImGui::PopID();
        ImGui::PushID("Viewmodel offset Y");
        ImGui::SliderFloat("", &config->misc.viewmodelChanger.y, -10.f, 10.f, "Viewmodel offset Y: %.2f");
        ImGui::PopID();
        ImGui::PushID("Viewmodel offset Z");
        ImGui::SliderFloat("", &config->misc.viewmodelChanger.z, -10.f, 10.f, "Viewmodel offset Z: %.2f");
        ImGui::PopID();
        ImGui::PushID("Viewmodel roll");
        ImGui::SliderInt("", &config->misc.viewmodelChanger.roll, -90, 90, "Viewmodel roll: %d");
        ImGui::PopID();
        ImGui::PopItemWidth();
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::Checkbox("Purchase List", &config->misc.purchaseList.enabled);
    ImGui::SameLine();

    ImGui::PushID("Purchase List");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::SetNextItemWidth(75.0f);
        ImGui::Combo("Mode", &config->misc.purchaseList.mode, "Details\0Summary\0");
        ImGui::Checkbox("Only During Freeze Time", &config->misc.purchaseList.onlyDuringFreezeTime);
        ImGui::Checkbox("Show Prices", &config->misc.purchaseList.showPrices);
        ImGui::Checkbox("No Title Bar", &config->misc.purchaseList.noTitleBar);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::Checkbox("Reportbot", &config->misc.reportbot.enabled);
    ImGui::SameLine();
    ImGui::PushID("Reportbot");

    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::PushItemWidth(80.0f);
        ImGui::Combo("Target", &config->misc.reportbot.target, "Enemies\0Allies\0All\0");
        ImGui::InputInt("Delay (s)", &config->misc.reportbot.delay);
        config->misc.reportbot.delay = (std::max)(config->misc.reportbot.delay, 1);
        ImGui::InputInt("Rounds", &config->misc.reportbot.rounds);
        config->misc.reportbot.rounds = (std::max)(config->misc.reportbot.rounds, 1);
        ImGui::PopItemWidth();
        ImGui::Checkbox("Abusive Communications", &config->misc.reportbot.textAbuse);
        ImGui::Checkbox("Griefing", &config->misc.reportbot.griefing);
        ImGui::Checkbox("Wall Hacking", &config->misc.reportbot.wallhack);
        ImGui::Checkbox("Aim Hacking", &config->misc.reportbot.aimbot);
        ImGui::Checkbox("Other Hacking", &config->misc.reportbot.other);
        if (ImGui::Button("Reset"))
            Misc::resetReportbot();
        ImGui::EndPopup();
    }
    ImGui::PopID();

    if (ImGui::Button("Unhook"))
        hooks->uninstall();

    ImGui::Columns(1);
    if (!contentOnly)
        ImGui::End();
}

void GUI::renderConfigWindow(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!window.config)
            return;
        ImGui::SetNextWindowSize({ 320.0f, 0.0f });
        if (!ImGui::Begin("Config", &window.config, windowFlags)) {
            ImGui::End();
            return;
        }
    }

    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 170.0f);

    static bool incrementalLoad = false;
    ImGui::Checkbox("Incremental Load", &incrementalLoad);

    ImGui::PushItemWidth(160.0f);

    auto& configItems = config->getConfigs();
    static int currentConfig = -1;

    static std::string buffer;

    timeToNextConfigRefresh -= ImGui::GetIO().DeltaTime;
    if (timeToNextConfigRefresh <= 0.0f) {
        config->listConfigs();
        if (const auto it = std::find(configItems.begin(), configItems.end(), buffer); it != configItems.end())
            currentConfig = std::distance(configItems.begin(), it);
        timeToNextConfigRefresh = 0.1f;
    }

    if (static_cast<std::size_t>(currentConfig) >= configItems.size())
        currentConfig = -1;

    if (ImGui::ListBox("", &currentConfig, [](void* data, int idx, const char** out_text) {
        auto& vector = *static_cast<std::vector<std::string>*>(data);
        *out_text = vector[idx].c_str();
        return true;
        }, &configItems, configItems.size(), 5) && currentConfig != -1)
            buffer = configItems[currentConfig];

        ImGui::PushID(0);
        if (ImGui::InputTextWithHint("", "config name", &buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (currentConfig != -1)
                config->rename(currentConfig, buffer.c_str());
        }
        ImGui::PopID();
        ImGui::NextColumn();

        ImGui::PushItemWidth(100.0f);

        if (ImGui::Button("Open config directory"))
            config->openConfigDir();

        if (ImGui::Button("Create config", { 100.0f, 25.0f }))
            config->add(buffer.c_str());

        if (ImGui::Button("Reset config", { 100.0f, 25.0f }))
            ImGui::OpenPopup("Config to reset");

        if (ImGui::BeginPopup("Config to reset")) {
            static constexpr const char* names[]{ "Whole", "Legitbot", "Ragebot", "Triggerbot", "Backtrack", "Anti aim", "Glow", "Chams", "ESP", "Visuals", "Skin changer", "Sound", "Style", "Misc" };
            for (int i = 0; i < IM_ARRAYSIZE(names); i++) {
                if (i == 1) ImGui::Separator();

                if (ImGui::Selectable(names[i])) {
                    switch (i) {
                    case 0: config->reset(); updateColors(); Misc::clanTag(true); SkinChanger::scheduleHudUpdate(); break;
                    case 1: config->legitbot = { }; break;
                    case 2: config->ragebot = { }; break;
                    case 3: config->triggerbot = { }; break;
                    case 4: Backtrack::resetConfig(); break;
                    case 5: AntiAim::resetConfig(); break;
                    case 6: Glow::resetConfig(); break;
                    case 7: config->chams = { }; break;
                    case 8: config->streamProofESP = { }; break;
                    case 9: config->visuals = { }; break;
                    case 10: config->skinChanger = { }; SkinChanger::scheduleHudUpdate(); break;
                    case 11: config->sound = { }; break;
                    case 12: config->style = { }; updateColors(); break;
                    case 13: config->misc = { };  Misc::clanTag(true); break;
                    }
                }
            }
            ImGui::EndPopup();
        }
        if (currentConfig != -1) {
            if (ImGui::Button("Load selected", { 100.0f, 25.0f })) {
                config->load(currentConfig, incrementalLoad);
                updateColors();
                SkinChanger::scheduleHudUpdate();
                Misc::clanTag(true);
            }
            if (ImGui::Button("Save selected", { 100.0f, 25.0f }))
                config->save(currentConfig);
            if (ImGui::Button("Delete selected", { 100.0f, 25.0f })) {
                config->remove(currentConfig);

                if (static_cast<std::size_t>(currentConfig) < configItems.size())
                    buffer = configItems[currentConfig];
                else
                    buffer.clear();
            }
        }
        ImGui::Columns(1);
        if (!contentOnly)
            ImGui::End();
}

void GUI::renderGuiStyle2() noexcept
{
    ImGui::Begin("Osiris", nullptr, windowFlags | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::BeginTabBar("TabBar", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_NoTooltip)) {
        if (ImGui::BeginTabItem("Legitbot")) {
            renderLegitbotWindow(true);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Ragebot")) {
            renderRagebotWindow(true);
            ImGui::EndTabItem();
        }
        AntiAim::tabItem();
        if (ImGui::BeginTabItem("Triggerbot")) {
            renderTriggerbotWindow(true);
            ImGui::EndTabItem();
        }
        Backtrack::tabItem();
        Glow::tabItem();
        if (ImGui::BeginTabItem("Chams")) {
            renderChamsWindow(true);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ESP")) {
            renderStreamProofESPWindow(true);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Visuals")) {
            renderVisualsWindow(true);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Skin changer")) {
            renderSkinChangerWindow(true);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Sound")) {
            renderSoundWindow(true);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Style")) {
            renderStyleWindow(true);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Misc")) {
            renderMiscWindow(true);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Config")) {
            renderConfigWindow(true);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

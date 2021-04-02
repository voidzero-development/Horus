#pragma once

enum class FrameStage;
class GameEvent;
struct ImDrawList;
struct UserCmd;
struct Vector;

namespace Misc
{
    void blockBot(UserCmd* cmd) noexcept;
    void edgejump(UserCmd* cmd) noexcept;
    void slowwalk(UserCmd* cmd) noexcept;
    void inverseRagdollGravity() noexcept;
    void clanTag(bool = false) noexcept;
    void spectatorList() noexcept;
    void forceCrosshair() noexcept;
    void recoilCrosshair() noexcept;
    void watermark() noexcept;
    void prepareRevolver(UserCmd*) noexcept;
    void fastPlant(UserCmd*) noexcept;
    void fastStop(UserCmd*) noexcept;
    void drawBombTimer() noexcept;
    void stealNames() noexcept;
    void disablePanoramablur() noexcept;
    bool changeName(bool, const char*, float) noexcept;
    void bunnyHop(UserCmd*) noexcept;
    void fixTabletSignal() noexcept;
    void fakePrime() noexcept;
    void killMessage(GameEvent& event) noexcept;
    void fixMovement(UserCmd* cmd, float yaw) noexcept;
    void slideFix(UserCmd* cmd, float yaw) noexcept;
    void antiAfkKick(UserCmd* cmd) noexcept;
    void fixAnimationLOD(FrameStage stage) noexcept;
    bool shouldAimStep() noexcept;
    void autoPistol(UserCmd* cmd) noexcept;
    void revealRanks(UserCmd* cmd) noexcept;
    void legitStrafer(UserCmd* cmd) noexcept;
    float rageStrafer(UserCmd* cmd, const Vector& currentViewAngles) noexcept;
    void removeCrouchCooldown(UserCmd* cmd) noexcept;
    void moonwalk(UserCmd* cmd) noexcept;
    void playHitSound(GameEvent& event) noexcept;
    void killSound(GameEvent& event) noexcept;
    void purchaseList(GameEvent* event = nullptr) noexcept;
    void oppositeHandKnife(FrameStage stage) noexcept;
    void runReportbot() noexcept;
    void resetReportbot() noexcept;
    void viewmodelChanger(FrameStage stage) noexcept;
    void preserveKillfeed(bool roundStart = false) noexcept;
    void voteRevealer(GameEvent& event) noexcept;
    void drawOffscreenEnemies(ImDrawList* drawList) noexcept;
    void autoAccept(const char* soundEntry) noexcept;
    void autoGG() noexcept;
    void deathmatchGod() noexcept;
    void chatSpammer() noexcept;
    void antiAimLines() noexcept;
    void forceRelayCluster() noexcept;

    void updateInput() noexcept;
}

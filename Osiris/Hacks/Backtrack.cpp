#include "Backtrack.h"
#include "Aimbot.h"
#include "../Config.h"
#include "../SDK/ConVar.h"
#include "../SDK/Entity.h"
#include "../SDK/FrameStage.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/LocalPlayer.h"
#include "../SDK/NetworkChannel.h"
#include "../SDK/UserCmd.h"

#if OSIRIS_BACKTRACK()

struct BacktrackConfig {
    bool enabled = false;
    bool ignoreSmoke = false;
    int timeLimit = 200;
} backtrackConfig;

static std::array<std::deque<Backtrack::Record>, 65> records;
std::deque<Backtrack::IncomingSequence> Backtrack::incomingSequences;

struct Cvars {
    ConVar* updateRate;
    ConVar* maxUpdateRate;
    ConVar* interp;
    ConVar* interpRatio;
    ConVar* minInterpRatio;
    ConVar* maxInterpRatio;
    ConVar* maxUnlag;
};

static Cvars cvars;

int Backtrack::timeToTicks(float time) noexcept
{
    return static_cast<int>(0.5f + time / memory->globalVars->intervalPerTick);
}

float Backtrack::getExtraTicks() noexcept
{
    auto network = interfaces->engine->getNetworkChannel();
    if (!network)
        return 0.f;

    return std::clamp(network->getLatency(1) - network->getLatency(0), 0.f, 0.2f);
}

void Backtrack::update(FrameStage stage) noexcept
{
    if (stage == FrameStage::RENDER_START) {
        if (!backtrackConfig.enabled || !localPlayer || !localPlayer->isAlive()) {
            for (auto& record : records)
                record.clear();
            return;
        }

        for (int i = 1; i <= interfaces->engine->getMaxClients(); i++) {
            auto entity = interfaces->entityList->getEntity(i);
            if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive() || !entity->isOtherEnemy(localPlayer.get())) {
                records[i].clear();
                continue;
            }

            if (!records[i].empty() && (records[i].front().simulationTime == entity->simulationTime()))
                continue;

            Record record{ };
            if (const Model* mod = entity->getModel(); mod)
                record.hdr = interfaces->modelInfo->getStudioModel(mod);
            record.head = entity->getBonePosition(8);
            record.origin = entity->getAbsOrigin();
            record.simulationTime = entity->simulationTime();
            record.simulationTime = entity->simulationTime();
            record.mins = entity->getCollideable()->obbMins();
            record.max = entity->getCollideable()->obbMaxs();

            entity->setupBones(record.matrix, 256, 0x7FF00, memory->globalVars->currenttime);

            records[i].push_front(record);

            while (records[i].size() > 3 && records[i].size() > static_cast<size_t>(timeToTicks(std::clamp(static_cast<float>(backtrackConfig.timeLimit) / 1000.f, 0.f, 0.2f + getExtraTicks()))))
                records[i].pop_back();

            if (auto invalid = std::find_if(std::cbegin(records[i]), std::cend(records[i]), [](const Record & rec) { return !valid(rec.simulationTime); }); invalid != std::cend(records[i]))
                records[i].erase(invalid, std::cend(records[i]));
        }
    }
}

float Backtrack::getLerp() noexcept
{
    auto ratio = std::clamp(cvars.interpRatio->getFloat(), cvars.minInterpRatio->getFloat(), cvars.maxInterpRatio->getFloat());
    return (std::max)(cvars.interp->getFloat(), (ratio / ((cvars.maxUpdateRate) ? cvars.maxUpdateRate->getFloat() : cvars.updateRate->getFloat())));
}

void Backtrack::run(UserCmd* cmd) noexcept
{
    if (!backtrackConfig.enabled)
        return;

    if (!(cmd->buttons & UserCmd::IN_ATTACK))
        return;

    if (!localPlayer)
        return;

    auto localPlayerEyePosition = localPlayer->getEyePosition();

    auto bestFov{ 255.f };
    Entity * bestTarget{ };
    int bestTargetIndex{ };
    Vector bestTargetHead{ };
    int bestRecord{ };

    const auto aimPunch = localPlayer->getAimPunch();

    for (int i = 1; i <= interfaces->engine->getMaxClients(); i++) {
        auto entity = interfaces->entityList->getEntity(i);
        if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive()
            || !entity->isOtherEnemy(localPlayer.get()))
            continue;

        const auto head = entity->getBonePosition(8);

        auto angle = Aimbot::calculateRelativeAngle(localPlayerEyePosition, head, cmd->viewangles + aimPunch);
        auto fov = std::hypotf(angle.x, angle.y);
        if (fov < bestFov) {
            bestFov = fov;
            bestTarget = entity;
            bestTargetIndex = i;
            bestTargetHead = head;
        }
    }

    if (bestTarget) {
        if (records[bestTargetIndex].size() <= 3 || (!backtrackConfig.ignoreSmoke && memory->lineGoesThroughSmoke(localPlayer->getEyePosition(), bestTargetHead, 1)))
            return;

        bestFov = 255.f;

        for (size_t i = 0; i < records[bestTargetIndex].size(); i++) {
            const auto& record = records[bestTargetIndex][i];
            if (!valid(record.simulationTime))
                continue;

            auto angle = Aimbot::calculateRelativeAngle(localPlayerEyePosition, record.head, cmd->viewangles + aimPunch);
            auto fov = std::hypotf(angle.x, angle.y);
            if (fov < bestFov) {
                bestFov = fov;
                bestRecord = i;
            }
        }
    }

    if (bestRecord) {
        const auto& record = records[bestTargetIndex][bestRecord];
        cmd->tickCount = timeToTicks(record.simulationTime + getLerp());
    }
}

void Backtrack::addLatencyToNetwork(NetworkChannel* network, float latency) noexcept
{
    for (auto& sequence : incomingSequences)
    {
        if (memory->globalVars->serverTime() - sequence.serverTime >= latency)
        {
            network->inReliableState = sequence.inReliableState;
            network->inSequenceNr = sequence.sequenceNr;
            break;
        }
    }
}

void Backtrack::updateIncomingSequences(bool reset) noexcept
{
    static float lastIncomingSequenceNumber = 0.f;

    if (reset)
        lastIncomingSequenceNumber = 0.f;

    if (!config->misc.fakeLatency.enabled)
        return;

    if (!localPlayer)
        return;

    auto network = interfaces->engine->getNetworkChannel();
    if (!network)
        return;

    if (network->inSequenceNr > lastIncomingSequenceNumber)
    {
        lastIncomingSequenceNumber = network->inSequenceNr;

        IncomingSequence sequence{ };
        sequence.inReliableState = network->inReliableState;
        sequence.sequenceNr = network->inSequenceNr;
        sequence.serverTime = memory->globalVars->serverTime();
        incomingSequences.push_front(sequence);
    }

    while (incomingSequences.size() > 2048)
        incomingSequences.pop_back();
}

const std::deque<Backtrack::Record>* Backtrack::getRecords(std::size_t index) noexcept
{
    if (!backtrackConfig.enabled)
        return nullptr;
    return &records[index];
}

bool Backtrack::valid(float simtime) noexcept
{
    const auto network = interfaces->engine->getNetworkChannel();
    if (!network)
        return false;

    auto delta = std::clamp(network->getLatency(0) + network->getLatency(1) + getLerp(), 0.f, cvars.maxUnlag->getFloat()) - (memory->globalVars->serverTime() - simtime);
    return std::abs(delta) <= 0.2f;
}

void Backtrack::init() noexcept
{
    cvars.updateRate = interfaces->cvar->findVar("cl_updaterate");
    cvars.maxUpdateRate = interfaces->cvar->findVar("sv_maxupdaterate");
    cvars.interp = interfaces->cvar->findVar("cl_interp");
    cvars.interpRatio = interfaces->cvar->findVar("cl_interp_ratio");
    cvars.minInterpRatio = interfaces->cvar->findVar("sv_client_min_interp_ratio");
    cvars.maxInterpRatio = interfaces->cvar->findVar("sv_client_max_interp_ratio");
    cvars.maxUnlag = interfaces->cvar->findVar("sv_maxunlag");
}

static bool backtrackWindowOpen = false;

void Backtrack::menuBarItem() noexcept
{
    if (ImGui::MenuItem("Backtrack")) {
        backtrackWindowOpen = true;
        ImGui::SetWindowFocus("Backtrack");
        ImGui::SetWindowPos("Backtrack", { 100.0f, 100.0f });
    }
}

void Backtrack::tabItem() noexcept
{
    if (ImGui::BeginTabItem("Backtrack")) {
        drawGUI(true);
        ImGui::EndTabItem();
    }
}

void Backtrack::drawGUI(bool contentOnly) noexcept
{
    if (!contentOnly) {
        if (!backtrackWindowOpen)
            return;
        ImGui::SetNextWindowSize({ 0.0f, 0.0f });
        ImGui::Begin("Backtrack", &backtrackWindowOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    }
    ImGui::Checkbox("Enabled", &backtrackConfig.enabled);
    ImGui::Checkbox("Ignore smoke", &backtrackConfig.ignoreSmoke);
    ImGui::PushItemWidth(220.0f);
    ImGui::SliderInt("Time limit", &backtrackConfig.timeLimit, 1, config->misc.fakeLatency.enabled ? 200 + config->misc.fakeLatency.amount : 200, "%d ms");
    backtrackConfig.timeLimit = std::clamp(backtrackConfig.timeLimit, 1, config->misc.fakeLatency.enabled ? 200 + config->misc.fakeLatency.amount : 200);
    ImGui::PopItemWidth();
    if (!contentOnly)
        ImGui::End();
}

static void to_json(json& j, const BacktrackConfig& o, const BacktrackConfig& dummy = {})
{
    WRITE("Enabled", enabled);
    WRITE("Ignore smoke", ignoreSmoke);
    WRITE("Time limit", timeLimit);
}

json Backtrack::toJson() noexcept
{
    json j;
    to_json(j, backtrackConfig);
    return j;
}

static void from_json(const json& j, BacktrackConfig& b)
{
    read(j, "Enabled", b.enabled);
    read(j, "Ignore smoke", b.ignoreSmoke);
    read(j, "Time limit", b.timeLimit);
}

void Backtrack::fromJson(const json& j) noexcept
{
    from_json(j, backtrackConfig);
}

void Backtrack::resetConfig() noexcept
{
    backtrackConfig = {};
}

#else

namespace Backtrack
{
    void update(FrameStage) noexcept {}
    void run(UserCmd*) noexcept {}

    const std::deque<Record>* getRecords(std::size_t index) noexcept { return nullptr; }
    bool valid(float simtime) noexcept { return false; }
    void init() noexcept {}

    // GUI
    void menuBarItem() noexcept {}
    void tabItem() noexcept {}
    void drawGUI(bool contentOnly) noexcept {}

    // Config
    json toJson() noexcept { return {}; }
    void fromJson(const json& j) noexcept {}
    void resetConfig() noexcept {}
}

#endif

#pragma once

#include <array>
#include <deque>

#include "../ConfigStructs.h"

#include "../SDK/matrix3x4.h"
#include "../SDK/Vector.h"
#include "../SDK/StudioRender.h"
#include "../SDK/ModelInfo.h"
#include "../SDK/NetworkChannel.h"

enum class FrameStage;
struct UserCmd;

#define OSIRIS_BACKTRACK() true

namespace Backtrack
{
    int timeToTicks(float time) noexcept;
    float getLerp() noexcept;
    float getExtraTicks() noexcept;

    void update(FrameStage) noexcept;
    void run(UserCmd*) noexcept;

    void addLatencyToNetwork(NetworkChannel*, float) noexcept;
    void updateIncomingSequences(bool reset = false) noexcept;

    struct Record {
        StudioHdr* hdr;
        Vector head;
        Vector origin;
        Vector max;
        Vector mins;
        float simulationTime;
        matrix3x4 matrix[256];
    };

    struct IncomingSequence
    {
        int inReliableState;
        int sequenceNr;
        float serverTime;
    };

    extern std::deque<IncomingSequence> incomingSequences;

    const std::deque<Record>* getRecords(std::size_t index) noexcept;
    bool valid(float simtime) noexcept;
    void init() noexcept;

    // GUI
    void menuBarItem() noexcept;
    void tabItem() noexcept;
    void drawGUI(bool contentOnly) noexcept;

    // Config
    json toJson() noexcept;
    void fromJson(const json& j) noexcept;
    void resetConfig() noexcept;
}

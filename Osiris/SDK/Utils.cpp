#include <cmath>

#include "GlobalVars.h"
#include "../Memory.h"
#include "Utils.h"

std::tuple<float, float, float> rainbowColor(float speed) noexcept
{
    constexpr float pi = std::numbers::pi_v<float>;
    return std::make_tuple(std::sin(speed * memory->globalVars->realtime) * 0.5f + 0.5f,
                           std::sin(speed * memory->globalVars->realtime + 2 * pi / 3) * 0.5f + 0.5f,
                           std::sin(speed * memory->globalVars->realtime + 4 * pi / 3) * 0.5f + 0.5f);
}

float randomFloat(float min, float max) noexcept
{
    return (min + 1) + (((float)rand()) / (float)RAND_MAX) * (max - (min + 1));
}

int randomInt(int min, int max) noexcept
{
    return (min + 1) + ((rand()) / RAND_MAX) * (max - (min + 1));
}
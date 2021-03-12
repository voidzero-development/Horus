#pragma once

#include "../imgui/imgui.h"

#include "../SDK/EngineTrace.h"
#include "../SDK/UserCmd.h"
#include "../SDK/Vector.h"

#include <array>

namespace GrenadePrediction {
	void run(UserCmd* cmd) noexcept;
	void draw() noexcept;
};
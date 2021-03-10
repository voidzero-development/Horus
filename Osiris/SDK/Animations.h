#pragma once
#include "../SDK/matrix3x4.h"
#include "../SDK/Vector.h"

struct UserCmd;

namespace Animations
{
	void update(UserCmd*, bool& sendPacket) noexcept;
	void real() noexcept;
	void fake() noexcept;
	struct Data
	{
		bool sendPacket;
		bool gotMatrix;
		Vector viewAngles;
		matrix3x4 fakeMatrix[256];
	};
	extern Data data;
}
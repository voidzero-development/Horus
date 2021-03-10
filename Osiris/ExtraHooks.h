#pragma once
#include "Hooks.h"
#include "Hooks/VmtSwap.h"
#include "SDK/Entity.h"

struct Container {
	Container() : isHooked(false) { }

	VmtSwap vmt;
	bool isHooked;
};

class ExtraHooks
{
public:
	Container player;
	void hookEntity(Entity* ent);
	bool init();
	void restore();
};

extern ExtraHooks extraHook;
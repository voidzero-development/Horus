#include "extraHooks.h"
#include "SDK/ModelInfo.h"

typedef float Quaternion[4];

ExtraHooks extraHook;

void __fastcall doExtraBoneProcessing(Entity* player, uint32_t, StudioHdr* hdr, Vector* pos, Quaternion* q, matrix3x4* matrix, void* boneList, void* context)
{
	const auto entity = reinterpret_cast<Entity*> (player);;
	auto state = entity->getAnimstate();
	const auto val = reinterpret_cast<float*> (reinterpret_cast<uintptr_t> (state) + 292);
	const auto backup = *val;
	auto backupOnGround = false;

	if (state)
	{
		backupOnGround = state->onGround;
		state->onGround = false;

		if (entity->velocity().length2D() < 0.1f)
			*val = 0.f;
	}

	extraHook.player.vmt.getOriginal<void>(197, player, hdr, pos, q, matrix, boneList, context);

	if (state)
	{
		*val = backup;
		state->onGround = backupOnGround;
	}
}

void __fastcall standardBlendingRules(Entity* player, uint32_t edx, StudioHdr* hdr, Vector* pos, Quaternion* q, float curTime, int boneMask)
{
	auto orig = extraHook.player.vmt.getOriginal<void, StudioHdr*, Vector*, Quaternion*, float, int>(205, hdr, pos, q, curTime, boneMask);
	uint32_t* effects = (uint32_t*)((uintptr_t)player + 0xF0);
	*effects |= 8;
	orig(player, hdr, pos, q, curTime, boneMask);
	*effects &= ~8;
}

void ExtraHooks::hookEntity(Entity* ent)
{
	player.vmt.init(ent);
	player.vmt.hookAt(197, doExtraBoneProcessing);
	player.vmt.hookAt(205, standardBlendingRules);
	player.isHooked = true;
}

bool ExtraHooks::init()
{
	if (interfaces->engine->isInGame())
	{
		if (!localPlayer || !localPlayer.get())
			return false;
		static Entity* oldLocalPlayer = nullptr;
		if (!player.isHooked || oldLocalPlayer != localPlayer.get())
		{
			oldLocalPlayer = localPlayer.get();
			hookEntity(localPlayer.get());
		}
	}
	return true;
}

void ExtraHooks::restore()
{
	if (player.isHooked)
	{
		player.vmt.restore();
		player.isHooked = false;
	}
}
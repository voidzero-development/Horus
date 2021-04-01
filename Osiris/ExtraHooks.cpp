#include "ExtraHooks.h"
#include "SDK/ModelInfo.h"

typedef float quaternion[4];

ExtraHooks extraHook;

static void __fastcall doExtraBoneProcessing(Entity* entity, uint32_t edx, StudioHdr* hdr, Vector* pos, quaternion* q, matrix3x4* matrix, void* boneList, void* context) noexcept
{
	auto state = entity->getAnimstate();

	auto backupOnGround = false;
	if (state)
	{
		backupOnGround = state->OnGround;
		state->OnGround = false;
	}

	extraHook.player[entity->index()].vmt.getOriginal<void>(197, entity, hdr, pos, q, matrix, boneList, context);

	if (state)
	{
		state->OnGround = backupOnGround;
	}
}

static void __fastcall standardBlendingRules(Entity* entity, uint32_t edx, StudioHdr* hdr, Vector* pos, quaternion* q, float curTime, int boneMask) noexcept
{
	auto original = extraHook.player[entity->index()].vmt.getOriginal<void, StudioHdr*, Vector*, quaternion*, float, int>(205, hdr, pos, q, curTime, boneMask);
	*entity->getEffects() |= 8;
	original(entity, hdr, pos, q, curTime, boneMask);
	*entity->getEffects() &= ~8;
}

void ExtraHooks::hookEntity(Entity* entity) noexcept
{
	int i = entity->index();
	player[i].vmt.init(entity);
	player[i].vmt.hookAt(197, doExtraBoneProcessing);
	player[i].vmt.hookAt(205, standardBlendingRules);
	player[i].isHooked = true;
}

void ExtraHooks::init()
{
	if (!interfaces->engine->isInGame())
	{
		for (size_t i = 0; i < player.size(); i++)
		{
			if (player[i].isHooked)
			{
				player[i].isHooked = false;
				player[i].entity = nullptr;
			}
		}
	}
	if (interfaces->engine->isInGame())
	{
		if (!localPlayer || !localPlayer->isAlive())
			return;

		for (int i = 1; i <= interfaces->engine->getMaxClients(); i++)
		{
			auto entity = interfaces->entityList->getEntity(i);
			if (!entity || !entity->isAlive() || !entity->isPlayer())
				continue;

			if (!player[i].isHooked || player[i].entity != entity)
			{
				player[i].entity = entity;
				hookEntity(entity);
			}
		}
	}
}

void ExtraHooks::restore()
{
	for (size_t i = 0; i < player.size(); i++)
	{
		player[i].isHooked = false;
		player[i].vmt.restore();
	}
}
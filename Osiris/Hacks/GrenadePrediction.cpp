#include "../imgui/imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../imgui/imgui_internal.h"

#include "GrenadePrediction.h"

#include "../SDK/Cvar.h"
#include "../SDK/ConVar.h"
#include "../SDK/Entity.h"
#include "../SDK/GlobalVars.h"

#include "../Config.h"
#include "../GameData.h"
#include "../Helpers.h"
#include "../Memory.h"
#include "../Interfaces.h"

#include <mutex>

std::vector<std::pair<ImVec2, ImVec2>> screenPoints;
std::vector<std::pair<ImVec2, ImVec2>> savedPoints;
std::vector<ImVec2> bouncePoints;

std::mutex renderMutex;

int grenadeAct{ 1 };

static bool worldToScreen(const Vector& in, ImVec2& out) noexcept
{
    const auto& matrix = GameData::toScreenMatrix();

    const auto w = matrix._41 * in.x + matrix._42 * in.y + matrix._43 * in.z + matrix._44;
    if (w < 0.001f)
        return false;

    out = ImGui::GetIO().DisplaySize / 2.0f;
    out.x *= 1.0f + (matrix._11 * in.x + matrix._12 * in.y + matrix._13 * in.z + matrix._14) / w;
    out.y *= 1.0f - (matrix._21 * in.x + matrix._22 * in.y + matrix._23 * in.z + matrix._24) / w;
    out = ImFloor(out);
    return true;
}

void traceHull(Vector& src, Vector& end, Trace& tr)
{
	interfaces->engineTrace->traceRay({ src, end }, 0x200400B, { localPlayer.get() }, tr);
}

void setup(Vector& vecSrc, Vector& vecThrow, Vector viewangles)
{
	auto angleVectors = [](const Vector& angles, Vector* forward, Vector* right, Vector* up)
	{
		float sr, sp, sy, cr, cp, cy;

		sp = static_cast<float>(sin(double(angles.x) * 0.01745329251f));
		cp = static_cast<float>(cos(double(angles.x) * 0.01745329251f));
		sy = static_cast<float>(sin(double(angles.y) * 0.01745329251f));
		cy = static_cast<float>(cos(double(angles.y) * 0.01745329251f));
		sr = static_cast<float>(sin(double(angles.z) * 0.01745329251f));
		cr = static_cast<float>(cos(double(angles.z) * 0.01745329251f));

		if (forward)
		{
			forward->x = cp * cy;
			forward->y = cp * sy;
			forward->z = -sp;
		}

		if (right)
		{
			right->x = (-1 * sr * sp * cy + -1 * cr * -sy);
			right->y = (-1 * sr * sp * sy + -1 * cr * cy);
			right->z = -1 * sr * cp;
		}

		if (up)
		{
			up->x = (cr * sp * cy + -sr * -sy);
			up->y = (cr * sp * sy + -sr * cy);
			up->z = cr * cp;
		}
	};
	Vector angThrow = viewangles;
	float pitch = angThrow.x;

	if (pitch <= 90.0f)
	{
		if (pitch < -90.0f)
		{
			pitch += 360.0f;
		}
	}
	else
	{
		pitch -= 360.0f;
	}

	float a = pitch - (90.0f - fabs(pitch)) * 10.0f / 90.0f;
	angThrow.x = a;

	float flVel = 750.0f * 0.9f;

	static const float power[] = { 1.0f, 1.0f, 0.5f, 0.0f };
	float b = power[grenadeAct];
	b = b * 0.7f;
	b = b + 0.3f;
	flVel *= b;

	Vector vForward, vRight, vUp;
	angleVectors(angThrow, &vForward, &vRight, &vUp);

	vecSrc = localPlayer->getEyePosition();
	float off = (power[grenadeAct] * 12.0f) - 12.0f;
	vecSrc.z += off;

	Trace tr;
	Vector vecDest = vecSrc;
	vecDest += vForward * 22.0f;

	traceHull(vecSrc, vecDest, tr);

	Vector vecBack = vForward; vecBack *= 6.0f;
	vecSrc = tr.endpos;
	vecSrc -= vecBack;

	vecThrow = localPlayer->velocity(); vecThrow *= 1.25f;
	vecThrow += vForward * flVel;
}

int physicsClipVelocity(const Vector& in, const Vector& normal, Vector& out, float overbounce)
{
	static const float STOP_EPSILON = 0.1f;

	float    backoff;
	float    change;
	float    angle;
	int        i, blocked;

	blocked = 0;

	angle = normal[2];

	if (angle > 0)
	{
		blocked |= 1;        // floor
	}
	if (!angle)
	{
		blocked |= 2;        // step
	}

	backoff = in.dotProduct(normal) * overbounce;

	for (i = 0; i < 3; i++)
	{
		change = normal[i] * backoff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
		{
			out[i] = 0;
		}
	}

	return blocked;
}

void pushEntity(Vector& src, const Vector& move, Trace& tr)
{
	Vector vecAbsEnd = src;
	vecAbsEnd += move;
	traceHull(src, vecAbsEnd, tr);
}

void resolveFlyCollisionCustom(Trace& tr, Vector& vecVelocity, float interval)
{
	// Calculate elasticity
	float flSurfaceElasticity = 1.0;
	float flGrenadeElasticity = 0.45f;
	float flTotalElasticity = flGrenadeElasticity * flSurfaceElasticity;
	if (flTotalElasticity > 0.9f) flTotalElasticity = 0.9f;
	if (flTotalElasticity < 0.0f) flTotalElasticity = 0.0f;

	// Calculate bounce
	Vector vecAbsVelocity;
	physicsClipVelocity(vecVelocity, tr.plane.normal, vecAbsVelocity, 2.0f);
	vecAbsVelocity *= flTotalElasticity;

	float flSpeedSqr = vecAbsVelocity.squareLength();
	static const float flMinSpeedSqr = 20.0f * 20.0f;

	if (flSpeedSqr < flMinSpeedSqr)
	{
		vecAbsVelocity.x = 0.0f;
		vecAbsVelocity.y = 0.0f;
		vecAbsVelocity.z = 0.0f;
	}

	if (tr.plane.normal.z > 0.7f)
	{
		vecVelocity = vecAbsVelocity;
		vecAbsVelocity *= ((1.0f - tr.fraction) * interval);
		pushEntity(tr.endpos, vecAbsVelocity, tr);
	}
	else
	{
		vecVelocity = vecAbsVelocity;
	}
}

void addGravityMove(Vector& move, Vector& vel, float frametime, bool onground)
{
	Vector basevel{ 0.0f, 0.0f, 0.0f };

	move.x = (vel.x + basevel.x) * frametime;
	move.y = (vel.y + basevel.y) * frametime;

	if (onground)
	{
		move.z = (vel.z + basevel.z) * frametime;
	}
	else
	{
		float gravity = 800.0f * 0.4f;
		float newZ = vel.z - (gravity * frametime);
		move.z = ((vel.z + newZ) / 2.0f + basevel.z) * frametime;
		vel.z = newZ;
	}
}

enum Act
{
	ACT_NONE,
	ACT_THROW,
	ACT_LOB,
	ACT_DROP,
};

void tick(int buttons)
{
	bool in_attack = buttons & UserCmd::IN_ATTACK;
	bool in_attack2 = buttons & UserCmd::IN_ATTACK2;

	grenadeAct = (in_attack && in_attack2) ? ACT_LOB :
		(in_attack2) ? ACT_DROP :
		(in_attack) ? ACT_THROW :
		ACT_NONE;
}

bool isEnabled(UserCmd* cmd)
{
	if (!localPlayer || !localPlayer->isAlive())
		return false;
	tick(cmd->buttons);
	if (localPlayer->moveType() == MoveType::NOCLIP)
		return false;

	auto activeWeapon = localPlayer->getActiveWeapon();
	if (!activeWeapon || !activeWeapon->isGrenade())
		return false;

	return true;
}

bool checkDetonate(const Vector& vecThrow, const Trace& tr, int tick, float interval, Entity* activeWeapon)
{
	switch (activeWeapon->itemDefinitionIndex2())
	{
	case WeaponId::SmokeGrenade:
	case WeaponId::Decoy:
	case WeaponId::Diversion:
		if (vecThrow.length2D() < 0.1f)
		{
			int detTickMod = (int)(0.2f / interval);
			return !(tick % detTickMod);
		}
		return false;
	case WeaponId::Molotov:
	case WeaponId::Firebomb:
	case WeaponId::IncGrenade:
		if (tr.fraction != 1.0f && tr.plane.normal.z > 0.7f)
			return true;
	case WeaponId::Flashbang:
	case WeaponId::FragGrenade:
	case WeaponId::HeGrenade:
		return (float)tick * interval > 1.5f && !(tick % (int)(0.2f / interval));
	default:
		return false;
	}
}

void GrenadePrediction::run(UserCmd* cmd) noexcept
{
	renderMutex.lock();

	screenPoints.clear();
	bouncePoints.clear();

	if (!isEnabled(cmd))
	{
		renderMutex.unlock();
		return;
	}
	if (!localPlayer || !localPlayer->isAlive())
		return;

	auto activeWeapon = localPlayer->getActiveWeapon();
	if (!activeWeapon)
		return;

	Vector vecSrc, vecThrow;
	setup(vecSrc, vecThrow, cmd->viewangles);

	float interval = memory->globalVars->intervalPerTick;
	int logstep = static_cast<int>(0.05f / interval);
	int logtimer = 0;

	std::vector<Vector> path;

	for (unsigned int i = 0; i < path.max_size() - 1; ++i)
	{
		if (!logtimer)
			path.emplace_back(vecSrc);

		Vector move;
		addGravityMove(move, vecThrow, interval, false);

		// Push entity
		Trace tr;
		pushEntity(vecSrc, move, tr);

		int result = 0;
		if (checkDetonate(vecThrow, tr, i, interval, activeWeapon))
			result |= 1;

		if (tr.fraction != 1.0f)
		{
			result |= 2; // Collision!
			resolveFlyCollisionCustom(tr, vecThrow, interval);

			ImVec2 bounce;

			if (worldToScreen(tr.endpos, bounce))
			{
				bouncePoints.emplace_back(bounce);
			}
		}

		vecSrc = tr.endpos;

		if (result & 1)
			break;

		if ((result & 2) || logtimer >= logstep)
			logtimer = 0;
		else
			++logtimer;
	}

	path.emplace_back(vecSrc);

	Vector prev = path[0];
	ImVec2 nadeStart, nadeEnd;
	Vector lastPos{ };
	for (auto& nade : path)
	{
		if (worldToScreen(prev, nadeStart) && worldToScreen(nade, nadeEnd))
		{
			screenPoints.emplace_back(std::pair<ImVec2, ImVec2>{ nadeStart, nadeEnd });
			prev = nade;
			lastPos = nade;
		}
	}

	renderMutex.unlock();
}

void GrenadePrediction::draw() noexcept
{
	if (!config->misc.grenadePrediction.enabled)
		return;

	if (!interfaces->engine->isInGame() || !localPlayer || !localPlayer->isAlive())
		return;

	if (renderMutex.try_lock()) {
		savedPoints = screenPoints;
		renderMutex.unlock();
	}

	if (savedPoints.empty())
		return;

	auto drawList = ImGui::GetBackgroundDrawList();

	//Draw nade path
	for (auto& point : savedPoints)
		drawList->AddLine(ImVec2(point.first.x, point.first.y), ImVec2(point.second.x, point.second.y), Helpers::calculateColor(config->misc.grenadePrediction.color));

	//Draw bounce points
	for (auto& point : bouncePoints)
		drawList->AddCircleFilled(ImVec2(point.x, point.y), 2.f, Helpers::calculateColor(config->misc.grenadePrediction.bounceColor));
}
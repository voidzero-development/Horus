#include <cassert>

#include "EventListener.h"
#include "fnv.h"
#include "GameData.h"
#include "Hacks/Legitbot.h"
#include "Hacks/Misc.h"
#include "Hacks/Resolver.h"
#include "Hacks/SkinChanger.h"
#include "Hacks/Visuals.h"
#include "Interfaces.h"
#include "Memory.h"

EventListener::EventListener() noexcept
{
    assert(interfaces);

    interfaces->gameEventManager->addListener(this, "item_purchase");
    interfaces->gameEventManager->addListener(this, "round_start");
    interfaces->gameEventManager->addListener(this, "round_freeze_end");
    interfaces->gameEventManager->addListener(this, "player_hurt");
    interfaces->gameEventManager->addListener(this, "weapon_fire");
    interfaces->gameEventManager->addListener(this, "bullet_impact");
    interfaces->gameEventManager->addListener(this, "cs_win_panel_match");

    interfaces->gameEventManager->addListener(this, "player_death");
    interfaces->gameEventManager->addListener(this, "vote_cast");

    if (const auto desc = memory->getEventDescriptor(interfaces->gameEventManager, "player_death", nullptr))
        std::swap(desc->listeners[0], desc->listeners[desc->listeners.size - 1]);
    else
        assert(false);
}

void EventListener::remove() noexcept
{
    assert(interfaces);

    interfaces->gameEventManager->removeListener(this);
}

void EventListener::fireGameEvent(GameEvent* event)
{
    switch (fnv::hashRuntime(event->getName())) {
    case fnv::hash("round_start"):
        GameData::clearProjectileList();
        Misc::preserveKillfeed(true);
        Resolver::update(event);
        break;
    case fnv::hash("item_purchase"):
    case fnv::hash("round_freeze_end"):
        Misc::purchaseList(event);
        break;
    case fnv::hash("player_death"):
        SkinChanger::updateStatTrak(*event);
        SkinChanger::overrideHudIcon(*event);
        Misc::killMessage(*event);
        Misc::killSound(*event);
        Legitbot::handleKill(*event);
        Resolver::update(event);
        break;
    case fnv::hash("weapon_fire"):
        Resolver::update(event);
        break;
    case fnv::hash("player_hurt"):
        Misc::playHitSound(*event);
        Visuals::hitEffect(event);
        Visuals::hitMarker(event);
        Resolver::update(event);
        break;
    case fnv::hash("bullet_impact"):
        Visuals::bulletTracer(*event);
        Resolver::update(event);
        break;
    case fnv::hash("cs_win_panel_match"):
        Misc::autoGG();
		break;
    case fnv::hash("vote_cast"):
        Misc::voteRevealer(*event);
        break;
    }
}

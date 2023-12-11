#include "pch.h"

#include "Actor.h"

#include "Game/Room.h"
#include "Game/Actor/ActorNetObserver.h"


Actor::Actor() noexcept :
	guid(0), ownerPlayerId(-1)
{
	
}


Actor::Actor(Actor&& other) noexcept
{
	guid = other.guid;
	ownerPlayerId = other.ownerPlayerId;
	observer = other.observer;
}


Actor& Actor::operator=(Actor&& other) noexcept
{
	if (&other == this) return *this;

	guid = other.guid;
	ownerPlayerId = other.ownerPlayerId;
	observer = other.observer;

	return *this;
}


Actor::~Actor()
{
	LOG_TRACE(FMT("액터 [{}] 제거됨! 소유자: [{}]", guid, ownerPlayerId));
}


void Actor::Initialize(ActorSpawnParams&& spawnParams, std::weak_ptr<Room> roomPtr)
{
	guid = spawnParams.transform.actorGuid;
	ownerPlayerId = spawnParams.playerId;

	// Observer 생성
	observer = std::make_shared<ActorNetObserver>(roomPtr);
	observer->Initialize(shared_from_this(), std::move(spawnParams));
}


void Actor::DestroyActor()
{
	if (observer == nullptr)
	{
		LOG_CRITICAL(FMT("액터 [{}]가 소멸하려는데, observer가 없음!!!!", guid));
		return;
	}

	LOG_TRACE(FMT("액터 [{}] DestroyActor 호출됨!", guid));
	observer->OnActorDestroyed(guid);
}

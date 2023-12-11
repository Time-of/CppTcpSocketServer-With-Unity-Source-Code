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
	LOG_TRACE(FMT("���� [{}] ���ŵ�! ������: [{}]", guid, ownerPlayerId));
}


void Actor::Initialize(ActorSpawnParams&& spawnParams, std::weak_ptr<Room> roomPtr)
{
	guid = spawnParams.transform.actorGuid;
	ownerPlayerId = spawnParams.playerId;

	// Observer ����
	observer = std::make_shared<ActorNetObserver>(roomPtr);
	observer->Initialize(shared_from_this(), std::move(spawnParams));
}


void Actor::DestroyActor()
{
	if (observer == nullptr)
	{
		LOG_CRITICAL(FMT("���� [{}]�� �Ҹ��Ϸ��µ�, observer�� ����!!!!", guid));
		return;
	}

	LOG_TRACE(FMT("���� [{}] DestroyActor ȣ���!", guid));
	observer->OnActorDestroyed(guid);
}

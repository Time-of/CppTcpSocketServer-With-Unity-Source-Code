#include "pch.h"

#include "ActorNetObserver.h"

#include "Game/Room.h"
#include "Game/Actor/Actor.h"
#include "Server/TcpServer.h"


ActorNetObserver::ActorNetObserver(std::weak_ptr<Room> roomPtr) noexcept :
	actorGuid(0), ownerPlayerId(-1), roomPtr(roomPtr)
{
	if (roomPtr.expired())
	{
		LOG_CRITICAL("room�� nullptr�ε� ActorNetObserver�� �����Ǿ���!!!");
		assert(false);
	}
}


ActorNetObserver::ActorNetObserver(ActorNetObserver&& other) noexcept
{
	actorGuid = other.actorGuid;
	ownerPlayerId = other.ownerPlayerId;
	roomPtr = other.roomPtr;
}


ActorNetObserver& ActorNetObserver::operator=(ActorNetObserver&& other) noexcept
{
	if (&other == this) return *this;
	actorGuid = other.actorGuid;
	ownerPlayerId = other.ownerPlayerId;
	roomPtr = other.roomPtr;
	return *this;
}


ActorNetObserver::~ActorNetObserver()
{
	LOG_TRACE(FMT("ActorNetObserver [{}] ���ŵ�! ������: [{}]", actorGuid, ownerPlayerId));
}


void ActorNetObserver::Initialize(std::shared_ptr<Actor> actor, ActorSpawnParams&& spawnParams)
{
	if (actor == nullptr)
	{
		LOG_CRITICAL("Actor�� nullptr�ε� ActorNetObserver�� �����Ǿ���!!!");
		assert(false);
	}

	actorGuid = actor->guid;
	ownerPlayerId = spawnParams.playerId;
	if (ownerPlayerId <= -1)
	{
		LOG_ERROR(FMT("�����ڰ� [{}]�� ActorNetObserver�� �����Ǿ���!", ownerPlayerId));
	}


	std::shared_ptr<Room> room = roomPtr.lock();

	// Room�� ���Ͱ� �����Ǿ����� �˸� (�÷��̾�鿡�� ������ ��� �뵵)
	if (room == nullptr)
	{
		LOG_CRITICAL("room�� nullptr�ε� ActorNetObserver�� �����Ǿ���!!!");
		assert(false);
	}

	room->ActorSpawnCompleted(std::move(spawnParams), actor);
}


void ActorNetObserver::AddListener(int playerId, const std::function<void(uint32)>& destroyed, const std::function<void(uint16, Byte*, uint8)>& syncActor)
{
	LOCKGUARD(delegateMutex, removeListenerLock);

	onActorDestroyedDelegates[playerId] = destroyed;
	syncActorUpdated[playerId] = syncActor;

	LOG_DEBUG(FMT("�÷��̾� [{}]���� ������ [{}] ���������� ��ϵ�!", playerId, actorGuid));
}


void ActorNetObserver::RemoveListener(int playerId, bool bAlreadyLocked)
{
	if (!bAlreadyLocked)
	{
		LOCKGUARD(delegateMutex, removeListenerLock);

		onActorDestroyedDelegates.erase(playerId);
		syncActorUpdated.erase(playerId);
	}
	else
	{
		onActorDestroyedDelegates.erase(playerId);
		syncActorUpdated.erase(playerId);
	}

	LOG_DEBUG(FMT("�÷��̾� [{}]���Լ� ������ [{}] ���������� ���ŵ�!", playerId, actorGuid));
}


void ActorNetObserver::OnActorDestroyed(uint32 destroyedActor)
{
	LOCKGUARD(delegateMutex, broadcastDestroyedMutex);

	std::vector<int> playerIds(onActorDestroyedDelegates.size());
	for (auto& delegatePair : onActorDestroyedDelegates)
	{
		if (delegatePair.second != nullptr)
		{
			LOG_TRACE(FMT("���� ������ [{}]�� [{}]���� ���Ͱ� �����Ǿ��ٰ� �뺸 ��...", actorGuid, delegatePair.first));

			delegatePair.second(destroyedActor); // ������ �����ȴٰ� �뺸
			playerIds.push_back(delegatePair.first);
		}
	}

	for (int ids : playerIds)
	{
		RemoveListener(ids, true);
	}
	LOG_TRACE(FMT("���� ������ [{}]�� ���� ���� �뺸 ����!", actorGuid));

	onActorDestroyedDelegates.clear();
}


void ActorNetObserver::NotifySyncActor(uint16 packetLength, Byte* packet, uint8 option)
{
	LOCKGUARD(delegateMutex, notifyPositionSyncDelegate);

	// ��-������ �÷��̾�鿡�� ������Ʈ�Ǿ����� �˸�
	for (auto& delegatePair : syncActorUpdated)
	{
		if (delegatePair.first == ownerPlayerId) continue;

		delegatePair.second(packetLength, packet, option);
	}
}


// ���� Ŭ���̾�Ʈ�� ActorObserver�� �����϶�� ����
// ������ PlayerClient�� Begin/End Listen�ϴ� �Ͱ� ���� �ֱ� ����
void ActorNetObserver::SyncBeginListeningToClient(SOCKET socket)
{
	auto room = roomPtr.lock();
	TransformInfo transformInfo;
	transformInfo.actorGuid = actorGuid;
	transformInfo.position = position;
	transformInfo.rotation = rotation;

	GetThreadPool->AddTask([room, socket, params = (std::move(transformInfo))]() mutable -> void
		{
			room->_HandleSpawnActorObserver(socket, std::move(params));
		});
}


void ActorNetObserver::SyncEndListenToClient(SOCKET socket)
{
	auto room = roomPtr.lock();
	//GetThreadPool->AddTask(&Room::_HandleDestroyActorObserver, room, socket, actorGuid);
	room->_HandleDestroyActorObserver(socket, actorGuid);
}

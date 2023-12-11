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
		LOG_CRITICAL("room이 nullptr인데 ActorNetObserver가 생성되었음!!!");
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
	LOG_TRACE(FMT("ActorNetObserver [{}] 제거됨! 소유자: [{}]", actorGuid, ownerPlayerId));
}


void ActorNetObserver::Initialize(std::shared_ptr<Actor> actor, ActorSpawnParams&& spawnParams)
{
	if (actor == nullptr)
	{
		LOG_CRITICAL("Actor가 nullptr인데 ActorNetObserver가 생성되었음!!!");
		assert(false);
	}

	actorGuid = actor->guid;
	ownerPlayerId = spawnParams.playerId;
	if (ownerPlayerId <= -1)
	{
		LOG_ERROR(FMT("소유자가 [{}]인 ActorNetObserver가 생성되었음!", ownerPlayerId));
	}


	std::shared_ptr<Room> room = roomPtr.lock();

	// Room에 액터가 스폰되었음을 알림 (플레이어들에게 리스닝 등록 용도)
	if (room == nullptr)
	{
		LOG_CRITICAL("room이 nullptr인데 ActorNetObserver가 생성되었음!!!");
		assert(false);
	}

	room->ActorSpawnCompleted(std::move(spawnParams), actor);
}


void ActorNetObserver::AddListener(int playerId, const std::function<void(uint32)>& destroyed, const std::function<void(uint16, Byte*, uint8)>& syncActor)
{
	LOCKGUARD(delegateMutex, removeListenerLock);

	onActorDestroyedDelegates[playerId] = destroyed;
	syncActorUpdated[playerId] = syncActor;

	LOG_DEBUG(FMT("플레이어 [{}]에게 옵저버 [{}] 정상적으로 등록됨!", playerId, actorGuid));
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

	LOG_DEBUG(FMT("플레이어 [{}]에게서 옵저버 [{}] 정상적으로 제거됨!", playerId, actorGuid));
}


void ActorNetObserver::OnActorDestroyed(uint32 destroyedActor)
{
	LOCKGUARD(delegateMutex, broadcastDestroyedMutex);

	std::vector<int> playerIds(onActorDestroyedDelegates.size());
	for (auto& delegatePair : onActorDestroyedDelegates)
	{
		if (delegatePair.second != nullptr)
		{
			LOG_TRACE(FMT("액터 옵저버 [{}]가 [{}]에게 액터가 삭제되었다고 통보 중...", actorGuid, delegatePair.first));

			delegatePair.second(destroyedActor); // 본인이 삭제된다고 통보
			playerIds.push_back(delegatePair.first);
		}
	}

	for (int ids : playerIds)
	{
		RemoveListener(ids, true);
	}
	LOG_TRACE(FMT("액터 옵저버 [{}]의 액터 삭제 통보 종료!", actorGuid));

	onActorDestroyedDelegates.clear();
}


void ActorNetObserver::NotifySyncActor(uint16 packetLength, Byte* packet, uint8 option)
{
	LOCKGUARD(delegateMutex, notifyPositionSyncDelegate);

	// 비-소유자 플레이어들에게 업데이트되었음을 알림
	for (auto& delegatePair : syncActorUpdated)
	{
		if (delegatePair.first == ownerPlayerId) continue;

		delegatePair.second(packetLength, packet, option);
	}
}


// 로컬 클라이언트에 ActorObserver를 생성하라고 지시
// 서버의 PlayerClient가 Begin/End Listen하는 것과 수명 주기 공유
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

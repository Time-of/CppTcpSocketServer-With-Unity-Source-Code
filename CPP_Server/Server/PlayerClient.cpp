#include "pch.h"

#include "PlayerClient.h"

#include "Game/Actor/Actor.h"
#include "Server/TcpServer.h"


PlayerClient::PlayerClient() noexcept
{
	bIsConnected = false;
	info.playerId = -1;
	state = EPlayerState::NONE;
	actorObserverMap.reserve(16);
}


PlayerClient::PlayerClient(PlayerClient&& other) noexcept
{
	bIsConnected = other.bIsConnected;
	state = other.state;
	connectSocket = std::move(other.connectSocket);
	info = std::move(other.info);
	actorObserverMap = std::move(other.actorObserverMap);
}


PlayerClient& PlayerClient::operator=(PlayerClient&& other) noexcept
{
	if (&other == this) return *this;

	bIsConnected = other.bIsConnected;
	state = other.state;
	connectSocket = std::move(other.connectSocket);
	info = std::move(other.info);
	actorObserverMap = std::move(other.actorObserverMap);

	return *this;
}


void PlayerClient::OnPlayerLeaveRoom()
{
	state = EPlayerState::NONE;

	// 모든 옵저버 제거
	LOG_TRACE(FMT("플레이어 [{}]가 방에서 나감으로 인해 옵저버 모두 제거 시도 중!", info.playerId));

	std::vector<uint32> foundGuids(actorObserverMap.size());

	for (auto observerPair : actorObserverMap)
	{
		if (!observerPair.second.expired())
		{
			foundGuids.push_back(observerPair.first);
		}
	}

	for (uint32 guid : foundGuids)
	{
		if (actorObserverMap.find(guid) == actorObserverMap.end())
		{
			LOG_ERROR(FMT("플레이어 [{}] actorObserverMap에서 [{}] 지우려 하였으나 보이지 않음!", info.playerId, guid));
			continue;
		}

		if (auto foundObserverPtr = actorObserverMap[guid].lock())
		{
			// 나가는 경우 EndSync는 보내지 않음
			foundObserverPtr->RemoveListener(info.playerId);
		}
	}

	actorObserverMap.clear();
}


void PlayerClient::BeginListenActorObserving(uint32 actorGuid, std::weak_ptr<ActorNetObserver> observer)
{
	std::shared_ptr<ActorNetObserver> observerPtr = observer.lock();
	if (observerPtr == nullptr)
	{
		LOG_CRITICAL(FMT("플레이어 [{}]: 액터 [{}]에 대해 Listen할 observer가 없음!!", info.playerId, actorGuid));
		return;
	}
	else if (info.playerId == -1 or !bIsConnected)
	{
		LOG_CRITICAL(FMT("플레이어 [{}]: 액터 [{}] 옵저버에 Listen 등록하려는데, 플레이어가 유효하지 않음!!", info.playerId, actorGuid));
		return;
	}

	auto foundObserver = actorObserverMap.find(actorGuid);
	if (foundObserver == actorObserverMap.end())
	{
		actorObserverMap[actorGuid] = observer;

		observerPtr->AddListener(info.playerId, 
			std::bind(&PlayerClient::RemoveListenActorObserver, this, std::placeholders::_1),
			std::bind(&PlayerClient::SendActorSync, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

		observerPtr->SyncBeginListeningToClient(connectSocket);
		LOG_TRACE(FMT("플레이어 [{}]에게 액터 [{}] 옵저버 등록", info.playerId, actorGuid));
	}
	else
	{
		LOG_WARN(FMT("플레이어 [{}]에게 액터 [{}] 옵저버가 이미 등록되어 있음!", info.playerId, actorGuid));
	}
}


void PlayerClient::RemoveListenActorObserver(uint32 actorGuid)
{
	LOG_TRACE(FMT("PlayerClient [{}]에서 액터 옵저버 [{}] 제거 시도...", info.playerId, actorGuid));
	auto foundObserver = actorObserverMap.find(actorGuid);
	
	if (foundObserver != actorObserverMap.end())
	{
		if (auto foundObserverPtr = foundObserver->second.lock())
		{
			LOG_TRACE(FMT("플레이어 [{}]에게서 액터 [{}] 옵저버 제거 중...", info.playerId, actorGuid));
			foundObserverPtr->SyncEndListenToClient(connectSocket);
			//foundObserverPtr->RemoveListener(info.playerId, true);
		}
	}
	
	actorObserverMap.erase(actorGuid);
}


void PlayerClient::SendActorSync(uint16 packetLength, Byte* packet, uint8 option)
{
	CVSPv2::Send(connectSocket, 
		CVSPv2::GetCommand(CVSPv2::Command::SyncActorTransform),
		option,
		packetLength, packet);
}
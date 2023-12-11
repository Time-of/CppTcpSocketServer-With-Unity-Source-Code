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

	// ��� ������ ����
	LOG_TRACE(FMT("�÷��̾� [{}]�� �濡�� �������� ���� ������ ��� ���� �õ� ��!", info.playerId));

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
			LOG_ERROR(FMT("�÷��̾� [{}] actorObserverMap���� [{}] ����� �Ͽ����� ������ ����!", info.playerId, guid));
			continue;
		}

		if (auto foundObserverPtr = actorObserverMap[guid].lock())
		{
			// ������ ��� EndSync�� ������ ����
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
		LOG_CRITICAL(FMT("�÷��̾� [{}]: ���� [{}]�� ���� Listen�� observer�� ����!!", info.playerId, actorGuid));
		return;
	}
	else if (info.playerId == -1 or !bIsConnected)
	{
		LOG_CRITICAL(FMT("�÷��̾� [{}]: ���� [{}] �������� Listen ����Ϸ��µ�, �÷��̾ ��ȿ���� ����!!", info.playerId, actorGuid));
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
		LOG_TRACE(FMT("�÷��̾� [{}]���� ���� [{}] ������ ���", info.playerId, actorGuid));
	}
	else
	{
		LOG_WARN(FMT("�÷��̾� [{}]���� ���� [{}] �������� �̹� ��ϵǾ� ����!", info.playerId, actorGuid));
	}
}


void PlayerClient::RemoveListenActorObserver(uint32 actorGuid)
{
	LOG_TRACE(FMT("PlayerClient [{}]���� ���� ������ [{}] ���� �õ�...", info.playerId, actorGuid));
	auto foundObserver = actorObserverMap.find(actorGuid);
	
	if (foundObserver != actorObserverMap.end())
	{
		if (auto foundObserverPtr = foundObserver->second.lock())
		{
			LOG_TRACE(FMT("�÷��̾� [{}]���Լ� ���� [{}] ������ ���� ��...", info.playerId, actorGuid));
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
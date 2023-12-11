#pragma once

#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <WinSock2.h>

#include "CVSP/CVSPv2PacketInfos.h"
#include "Game/Actor/ActorNetObserver.h"
#include "Serialize/Serializer.h"


enum class EPlayerState
{
	NONE = 0,
	IN_LOBBY,
	IN_ROOM,
	IN_TRANSITION
};


/**
* @author �̼���
* @brief �÷��̾��� ������ ���� ��ü�Դϴ�.
*/
class PlayerClient
{
public:
	explicit PlayerClient() noexcept;
	explicit PlayerClient(PlayerClient&& other) noexcept;
	PlayerClient& operator=(PlayerClient&& other) noexcept;
	~PlayerClient() = default;


public:
	bool bIsConnected = false;
	EPlayerState state;

	SOCKET connectSocket = SOCKET_ERROR;
	PlayerInfo info;


public:
	// Room���� ���� ������ ��� ����
	void BeginListenActorObserving(uint32 actorGuid, std::weak_ptr<ActorNetObserver> observer);

	// ���� ������ ����
	void RemoveListenActorObserver(uint32 actorGuid);


public:
	void OnPlayerLeaveRoom();


private:
	void SendActorSync(uint16 packetLength, Byte* packet, uint8 option);


private:
	std::unordered_map<uint32, std::weak_ptr<ActorNetObserver>> actorObserverMap;


private:
	explicit PlayerClient(const PlayerClient& other) = delete;
	PlayerClient& operator=(const PlayerClient& other) = delete;
};

using PlayerPtr = std::shared_ptr<PlayerClient>;
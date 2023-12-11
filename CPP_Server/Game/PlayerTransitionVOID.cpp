#include "pch.h"

#include "PlayerTransitionVOID.h"

#include "Game/Lobby.h"
#include "Server/TcpServer.h"


PlayerTransitionVOID::PlayerTransitionVOID(std::shared_ptr<Lobby> lobbyPtr) noexcept : lobby(lobbyPtr)
{
	if (lobby == nullptr)
	{
		LOG_CRITICAL("lobby 포인터가 유효하지 않음!!!");
	}
}


PlayerTransitionVOID::~PlayerTransitionVOID()
{

}


void PlayerTransitionVOID::EnqueuePlayer(PlayerPtr player, std::shared_ptr<INetworkTaskHandler> destination)
{
	if (player == nullptr)
	{
		LOG_CRITICAL("플레이어가 nullptr입니다!");
		return;
	}

	LOCKGUARD(transitionMutex, transitionLock);

	if (!player->bIsConnected or player->connectSocket == SOCKET_ERROR)
	{
		LOG_DEBUG("플레이어가 연결이 끊어졌거나 소켓 오류인데 이동시키려 함!");
		LOG_DEBUG("따라서 로비로 이동시킴!");
		GetThreadPool->AddTask(&PlayerTransitionVOID::_JoinLobby, this, player);
		return;
	}


	if (destination != nullptr)
	{
		LOG_INFO(FMT("플레이어 [{}]를 목적지로 이동시키기 위해 작업 큐에 넣음", player->info.playerId));
		GetThreadPool->AddTask(&PlayerTransitionVOID::_TryJoin, this, player, destination);
	}
	// nullptr이면 로비로 이동
	else
	{
		LOG_INFO(FMT("플레이어 [{}]를 로비로 이동시키기 위해 작업 큐에 넣음", player->info.playerId));
		GetThreadPool->AddTask(&PlayerTransitionVOID::_JoinLobby, this, player);
	}
}


void PlayerTransitionVOID::_TryJoin(PlayerPtr player, std::shared_ptr<INetworkTaskHandler> destination)
{
	if (player == nullptr)
	{
		LOG_CRITICAL("플레이어가 nullptr입니다!");
		return;
	}

	bool bResult = false;
	Room* room = dynamic_cast<Room*>(destination.get());
	if (room != nullptr and (room->GetState() == ERoomState::OPENED or room->GetState() == ERoomState::CREATED))
	{
		LOG_INFO(FMT("플레이어 [{}] 목적지로 이동 시도...", player->info.playerId));
		bResult = room->Join(player, false);

		if (bResult)
		{
			LOG_INFO(FMT("플레이어 [{}] 목적지로 이동 성공!!", player->info.playerId));
			
			return;
		}
	}
	

	LOG_ERROR(FMT("플레이어 [{}] 목적지로 이동 실패! 로비로 이동, JoinRoom 실패 메시지 전송!", player->info.playerId));
	
	_JoinLobby(player);
}


void PlayerTransitionVOID::_JoinLobby(PlayerPtr player)
{
	if (player == nullptr)
	{
		LOG_CRITICAL("플레이어가 nullptr입니다!");
		return;
	}

	LOG_INFO(FMT("플레이어 [{}]를 로비로 이동 중...", player->info.playerId));

	if (lobby == nullptr)
	{
		LOG_CRITICAL("로비 포인터가 유효하지 않음!!!!");
		return;
	}

	GetThreadPool->AddTask(&Lobby::Join, lobby, player, true);
}

#include "pch.h"

#include "PlayerTransitionVOID.h"

#include "Game/Lobby.h"
#include "Server/TcpServer.h"


PlayerTransitionVOID::PlayerTransitionVOID(std::shared_ptr<Lobby> lobbyPtr) noexcept : lobby(lobbyPtr)
{
	if (lobby == nullptr)
	{
		LOG_CRITICAL("lobby �����Ͱ� ��ȿ���� ����!!!");
	}
}


PlayerTransitionVOID::~PlayerTransitionVOID()
{

}


void PlayerTransitionVOID::EnqueuePlayer(PlayerPtr player, std::shared_ptr<INetworkTaskHandler> destination)
{
	if (player == nullptr)
	{
		LOG_CRITICAL("�÷��̾ nullptr�Դϴ�!");
		return;
	}

	LOCKGUARD(transitionMutex, transitionLock);

	if (!player->bIsConnected or player->connectSocket == SOCKET_ERROR)
	{
		LOG_DEBUG("�÷��̾ ������ �������ų� ���� �����ε� �̵���Ű�� ��!");
		LOG_DEBUG("���� �κ�� �̵���Ŵ!");
		GetThreadPool->AddTask(&PlayerTransitionVOID::_JoinLobby, this, player);
		return;
	}


	if (destination != nullptr)
	{
		LOG_INFO(FMT("�÷��̾� [{}]�� �������� �̵���Ű�� ���� �۾� ť�� ����", player->info.playerId));
		GetThreadPool->AddTask(&PlayerTransitionVOID::_TryJoin, this, player, destination);
	}
	// nullptr�̸� �κ�� �̵�
	else
	{
		LOG_INFO(FMT("�÷��̾� [{}]�� �κ�� �̵���Ű�� ���� �۾� ť�� ����", player->info.playerId));
		GetThreadPool->AddTask(&PlayerTransitionVOID::_JoinLobby, this, player);
	}
}


void PlayerTransitionVOID::_TryJoin(PlayerPtr player, std::shared_ptr<INetworkTaskHandler> destination)
{
	if (player == nullptr)
	{
		LOG_CRITICAL("�÷��̾ nullptr�Դϴ�!");
		return;
	}

	bool bResult = false;
	Room* room = dynamic_cast<Room*>(destination.get());
	if (room != nullptr and (room->GetState() == ERoomState::OPENED or room->GetState() == ERoomState::CREATED))
	{
		LOG_INFO(FMT("�÷��̾� [{}] �������� �̵� �õ�...", player->info.playerId));
		bResult = room->Join(player, false);

		if (bResult)
		{
			LOG_INFO(FMT("�÷��̾� [{}] �������� �̵� ����!!", player->info.playerId));
			
			return;
		}
	}
	

	LOG_ERROR(FMT("�÷��̾� [{}] �������� �̵� ����! �κ�� �̵�, JoinRoom ���� �޽��� ����!", player->info.playerId));
	
	_JoinLobby(player);
}


void PlayerTransitionVOID::_JoinLobby(PlayerPtr player)
{
	if (player == nullptr)
	{
		LOG_CRITICAL("�÷��̾ nullptr�Դϴ�!");
		return;
	}

	LOG_INFO(FMT("�÷��̾� [{}]�� �κ�� �̵� ��...", player->info.playerId));

	if (lobby == nullptr)
	{
		LOG_CRITICAL("�κ� �����Ͱ� ��ȿ���� ����!!!!");
		return;
	}

	GetThreadPool->AddTask(&Lobby::Join, lobby, player, true);
}

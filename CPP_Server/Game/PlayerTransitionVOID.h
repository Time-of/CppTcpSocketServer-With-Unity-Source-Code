#pragma once

#include "Game/Room.h"
#include "Server/PlayerClient.h"


class Lobby;


/**
* @author �̼���
* �÷��̾ �� - �κ� ���̸� �̵��ϱ� ���� �ӹ����� �ӽ� �����Դϴ�.
* ��ġ ����͵� ���� �����Դϴ�... �� ���� �ƹ��͵� �������� �ʽ��ϴ�.
* ThreadPool�� �̵� �۾��� �������ִ�, Helper Ŭ������ �������ϴ�.
*/
class PlayerTransitionVOID : public CannotCopyable
{
public:
	explicit PlayerTransitionVOID(std::shared_ptr<Lobby> lobbyPtr) noexcept;
	~PlayerTransitionVOID();


public:
	// @brief Ʈ������ �������� ���Խ�Ű��. ���� destination���� ��.
	// @brief **��Ȱ�� �۵��� ����, �����ϸ� �ٸ� mutex ���� �ۿ��� ȣ���� ��!!!**
	// @brief �ε����ϰ� mutex ���� ������ ȣ���ϴ� ���... LOG_DEBUG�� �α��س��� ��.
	// @param destination ��� ������. nullptr�̸� �κ�� ��.
	void EnqueuePlayer(PlayerPtr player, std::shared_ptr<INetworkTaskHandler> destination);


public:
	void _TryJoin(PlayerPtr player, std::shared_ptr<INetworkTaskHandler> destination);
	void _JoinLobby(PlayerPtr player);


private:
	std::mutex transitionMutex;
	std::shared_ptr<Lobby> lobby; // nullptr�̾�� �� ��!!


private:
	explicit PlayerTransitionVOID() noexcept = delete;
};
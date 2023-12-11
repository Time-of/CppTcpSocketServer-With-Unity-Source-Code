#pragma once

#include "Game/Room.h"
#include "Server/PlayerClient.h"


class Lobby;


/**
* @author 이성수
* 플레이어가 방 - 로비 사이를 이동하기 위해 머무르는 임시 영역입니다.
* 마치 공허와도 같은 영역입니다... 이 곳엔 아무것도 존재하지 않습니다.
* ThreadPool에 이동 작업을 지시해주는, Helper 클래스에 가깝습니다.
*/
class PlayerTransitionVOID : public CannotCopyable
{
public:
	explicit PlayerTransitionVOID(std::shared_ptr<Lobby> lobbyPtr) noexcept;
	~PlayerTransitionVOID();


public:
	// @brief 트랜지션 영역으로 진입시키기. 이후 destination으로 감.
	// @brief **원활한 작동을 위해, 가능하면 다른 mutex 범위 밖에서 호출할 것!!!**
	// @brief 부득이하게 mutex 범위 내에서 호출하는 경우... LOG_DEBUG로 로깅해놓을 것.
	// @param destination 대상 포인터. nullptr이면 로비로 감.
	void EnqueuePlayer(PlayerPtr player, std::shared_ptr<INetworkTaskHandler> destination);


public:
	void _TryJoin(PlayerPtr player, std::shared_ptr<INetworkTaskHandler> destination);
	void _JoinLobby(PlayerPtr player);


private:
	std::mutex transitionMutex;
	std::shared_ptr<Lobby> lobby; // nullptr이어서는 안 됨!!


private:
	explicit PlayerTransitionVOID() noexcept = delete;
};
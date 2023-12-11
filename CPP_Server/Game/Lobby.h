#pragma once

#include <deque>
#include <vector>
#include <stack>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "Types/MyTypes.h"
#include "Server/PlayerClient.h"
#include "Interface/INetworkTaskHandler.h"

class Room;
class TcpServer;


/**
* 로비 클래스.
* 플레이어 및 Room을 관리합니다.
*/
class Lobby : public INetworkTaskHandler, public std::enable_shared_from_this<Lobby>
{
public:
	explicit Lobby() noexcept;
	~Lobby();


public:
	void Receive() final;
	bool Join(PlayerPtr player, bool bForceJoin) final;
	bool Leave(PlayerPtr player) final;


public:
	PlayerPtr GetPlayerFromPool();
	void PushToPlayerPool(PlayerPtr playerIter);

	// @brief 현재 플레이어 풀의 size를 출력하는 디버그 기능.
	void PrintDebugPlayerPoolSize() const;

	void _HandlePlayerDisconnected(PlayerPtr playerIter);


private:
	// @brief 방 생성.
	// @param playerId 요청한 플레이어의 ID
	// @return 생성된 방의 GUID, 실패 시 0 반환 (GUID는 99.99% 확률로 0이 등장하지 않음)
	uint32 CreateRoom(PlayerPtr player, const CreateRoomInfo& roomInfo);
	void DestroyRoom(uint32 roomId);

	bool JoinRoom(PlayerPtr player, uint32 roomId);

	// 방 생성 후, 요청자를 방에 입장까지 처리시키는 함수.
	void _HandleCreateRoom(PlayerPtr player, CreateRoomInfo roomInfo);


public:
	// DestroyRoom()을 스레드 풀에 요청
	void RequestDestroyRoom(uint32 roomId);


private:
	// 여기에서 FD_SET, 가능하면 mutex 안에서 호출
	void OnPlayerJoinedLobby(PlayerPtr player);

	// 여기에서 FD_CLR, 반드시 lock 걸고 호출할 것!!
	void OnPlayerLeaveLobby(PlayerPtr player);


private:
	std::stack<PlayerPtr, std::vector<PlayerPtr>> playerPool;
	std::deque<PlayerPtr> activePlayers;

	std::mutex playerControlMutex;
	std::mutex roomControlMutex;

	std::unordered_map<uint32, std::shared_ptr<Room>> roomMap;


private:
	/** @brief 플레이어가 총 연결된 횟수. 플레이어는 해당 값을 ID로 사용한다. */
	int32 totalPlayerConnectedCount = 0;

	fd_set fdMaster;
	int maxFd = 0;


private:
	explicit Lobby(const Lobby& other) = delete;
	Lobby& operator=(const Lobby& other) = delete;
};
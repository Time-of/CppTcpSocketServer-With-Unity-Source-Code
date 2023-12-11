#pragma once

#include <unordered_map>

#include "Interface/INetworkTaskHandler.h"
#include "Server/PlayerClient.h"
#include "Types/CannotCopyable.h"
#include "Types/MyTypes.h"

class Actor;
class ActorNetObserver;
class Lobby;

enum class ERoomState
{
	CREATED, // 생성 직후. 플레이어가 없는 상태.
	OPENED, // 플레이어에게 열린 상태
	PENDING_DESTROY, // 파괴 대기 중
	GAME_STARTED // 게임 시작
};


class Room : public INetworkTaskHandler, public CannotCopyable, public std::enable_shared_from_this<Room>
{
public:
	explicit Room(const CreateRoomInfo& roomInfo, uint32 guid, std::shared_ptr<Lobby> lobbyPtr) noexcept;
	~Room();

	void Receive() final;
	bool Join(PlayerPtr player, bool bForceJoin) final;
	bool Leave(PlayerPtr player) final;


private:
	/* --- 게임 관련 기능들 --- */ 

	// 게임 시작
	void StartGame();


private:
	/* --- 인 게임 관련 기능들. --- */
	
	void _HandleSpawnActor(ActorSpawnParams&& spawnParams);
	void _HandleDestroyActor(uint32 actorGuid);
	void _HandleSyncActorTransform(uint8 option, uint16 packetLength, Byte* packet, uint32 actorGuid);


public:
	/* --- 인 게임 관련 기능들, Room 외부에서 수행. --- */

	// 플레이어를 액터 옵저빙 Notify Delegate 등록시키기
	void AddListenerToNotifyActorObserving(PlayerPtr player, uint32 actorGuid, std::weak_ptr<ActorNetObserver> observer);
	void RemoveListenerToNotifyActorObserving(PlayerPtr player, uint32 actorGuid, std::weak_ptr<ActorNetObserver> observer);

	// 서버에 액터 스폰되었다고 방에 알림 (ActorNetObserver가 수행)
	// 여기서 모든 플레이어에게 스폰을 지시.
	void ActorSpawnCompleted(ActorSpawnParams&& spawnParams, std::shared_ptr<Actor> spawnedServerActor);


	void _HandleSpawnActorObserver(SOCKET playerSocket, TransformInfo&& transformInfo);
	void _HandleDestroyActorObserver(SOCKET playerSocket, uint32 actorGuid);


private:
	// 플레이어의 씬 로드 핸들링 (스레드)
	void _HandlePlayerFinishLoadScene(PlayerPtr player, LoadSceneCompleteInfo sceneInfo);


private:
	/* --- 플레이어 입장/퇴장 --- */

	// 외부에서 lock 걸고 수행!
	void OnPlayerJoinedRoom(PlayerPtr player, int8 playerIndex); // 여기에서 FD_SET
	void OnPlayerLeaveRoom(PlayerPtr player); // 여기에서 FD_CLR


public:
	// 방 삭제 요청
	void NotifyRoomShouldBeDestroyed();


private:
	std::vector<PlayerPtr> activePlayers;
	std::mutex playerControlMutex;
	std::mutex actorMutex;

	fd_set fdMaster;
	int maxFd = 0;
	int32 roomMasterPlayerId; // 현재 방장 (게임 시작 권한)


public:
	ERoomState GetState() const { return state; }


private:
	ERoomState state;


private:
	bool ChangeState(ERoomState newState);


public:
	std::string roomName;
	uint32 roomGuid;
	uint8 validPlayerCount;


private:
	std::string gameSceneName = "GameScene";


private:
	/* --- 인 게임 변수들 --- */

	uint8 LoadSceneFinishedPlayerCount; // 게임 씬 불러오기에 성공한 플레이어 수
	std::unordered_map<uint32, std::shared_ptr<Actor>> actorMap;


private:
	/* --- 방 관련 변수들 --- */

	std::thread roomControlThread;
	std::mutex roomStateMutex; // 방 상태 조정 뮤텍스
	std::shared_ptr<Lobby> lobby;


private:
	explicit Room() noexcept = delete;
};
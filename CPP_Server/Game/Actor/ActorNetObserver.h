#pragma once

#include "CVSP/CVSPv2PacketInfos.h"

class Actor;
class Room;


/**
* PlayerClient와 Actor가 통신하기 위한 계층입니다.
* 주로, 옵저버 패턴과 비슷하게, 서버에 '해당 actor에 변화가 생겼음'을 전송받으면,
* ActorNetObserver는 '관련 정보를 통지' 합니다.
* 
* 해당 객체가 생성되었을 때, 방에서 생성한 플레이어 및 관련 있는 모든 플레이어에게 해당 객체를 참조하라고 알립니다.
* 액터의 생성 / 삭제 동기화는 Room에서 이루어지며, 해당 객체는 삭제 정보를 Actor로부터 전달받습니다.
*/
class ActorNetObserver
{
public:
	explicit ActorNetObserver(std::weak_ptr<Room> roomPtr) noexcept;
	explicit ActorNetObserver(ActorNetObserver&& other) noexcept;
	ActorNetObserver& operator=(ActorNetObserver&& other) noexcept;
	~ActorNetObserver();

	friend class Actor;


public:
	void Initialize(std::shared_ptr<Actor> actor, ActorSpawnParams&& spawnParams);

	void AddListener(int playerId, const std::function<void(uint32)>& destroyed, const std::function<void(uint16, Byte*, uint8)>& syncActor);
	void RemoveListener(int playerId, bool bAlreadyLocked = false);


private:
	// 액터가 제거될 때 호출
	// 이 옵저버의 통지를 받는 플레이어들에게서 옵저버를 제거시키도록 하기
	void OnActorDestroyed(uint32 destroyedActor);


public:
	void NotifySyncActor(uint16 packetLength, Byte* packet, uint8 option);


private:
	// 이 위치, 회전 정보는 ActorNetObserver가 클라이언트에 등장 시
	// 위치와 회전을 함께 전송하기 위한 용도이므로, Notify보다 우선도가 낮음.
	UnityVector3 position;
	UnityQuaternion rotation;


public:
	// PlayerClient에서 Begin/End Listen 시 함께 호출됨.
	/* --- 클라이언트와 ActorNetObserver 동기화 --- */

	void SyncBeginListeningToClient(SOCKET socket);
	void SyncEndListenToClient(SOCKET socket);


private:
	uint32 actorGuid;
	int ownerPlayerId;
	std::weak_ptr<Room> roomPtr;
	std::mutex delegateMutex;


public:
	std::unordered_map<int, std::function<void(uint32)>> onActorDestroyedDelegates;
	std::unordered_map<int, std::function<void(uint16, Byte*, uint8)>> syncActorUpdated;


private:
	explicit ActorNetObserver() noexcept = delete;
	explicit ActorNetObserver(const ActorNetObserver& other) = delete;
	ActorNetObserver& operator=(const ActorNetObserver& other) = delete;
};
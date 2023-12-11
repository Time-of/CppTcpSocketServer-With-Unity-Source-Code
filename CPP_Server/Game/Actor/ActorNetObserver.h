#pragma once

#include "CVSP/CVSPv2PacketInfos.h"

class Actor;
class Room;


/**
* PlayerClient�� Actor�� ����ϱ� ���� �����Դϴ�.
* �ַ�, ������ ���ϰ� ����ϰ�, ������ '�ش� actor�� ��ȭ�� ������'�� ���۹�����,
* ActorNetObserver�� '���� ������ ����' �մϴ�.
* 
* �ش� ��ü�� �����Ǿ��� ��, �濡�� ������ �÷��̾� �� ���� �ִ� ��� �÷��̾�� �ش� ��ü�� �����϶�� �˸��ϴ�.
* ������ ���� / ���� ����ȭ�� Room���� �̷������, �ش� ��ü�� ���� ������ Actor�κ��� ���޹޽��ϴ�.
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
	// ���Ͱ� ���ŵ� �� ȣ��
	// �� �������� ������ �޴� �÷��̾�鿡�Լ� �������� ���Ž�Ű���� �ϱ�
	void OnActorDestroyed(uint32 destroyedActor);


public:
	void NotifySyncActor(uint16 packetLength, Byte* packet, uint8 option);


private:
	// �� ��ġ, ȸ�� ������ ActorNetObserver�� Ŭ���̾�Ʈ�� ���� ��
	// ��ġ�� ȸ���� �Բ� �����ϱ� ���� �뵵�̹Ƿ�, Notify���� �켱���� ����.
	UnityVector3 position;
	UnityQuaternion rotation;


public:
	// PlayerClient���� Begin/End Listen �� �Բ� ȣ���.
	/* --- Ŭ���̾�Ʈ�� ActorNetObserver ����ȭ --- */

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
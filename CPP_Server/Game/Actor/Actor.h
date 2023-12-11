#pragma once

class ActorNetObserver;
class Room;


/**
* ���� ������ ��Ʈ��ũ ����ȭ�� �̷������ ��ü��.
* Ŭ���̾�Ʈ�� ���� �ֱ⸦ ����.
* PlayerClient�� ActorNetObserver�� ���� ���.
* 
* Actor ���� ��, ������ �����ϴ� ActorNetObserver�� �����մϴ�. (������)
* 
* @see ActorNetObserver ����.
*/
class Actor : public std::enable_shared_from_this<Actor>
{
public:
	explicit Actor() noexcept;
	explicit Actor(Actor&& other) noexcept;
	Actor& operator=(Actor&& other) noexcept;
	~Actor();


public:
	void Initialize(ActorSpawnParams&& spawnParams, std::weak_ptr<Room> roomPtr);

	void DestroyActor();


public:
	uint32 guid;
	int ownerPlayerId;

	std::shared_ptr<ActorNetObserver> observer;


private:
	explicit Actor(const Actor& other) = delete;
	Actor& operator=(const Actor& other) = delete;
};
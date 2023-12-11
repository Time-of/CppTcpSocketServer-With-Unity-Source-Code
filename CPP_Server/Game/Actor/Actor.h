#pragma once

class ActorNetObserver;
class Room;


/**
* 게임 씬에서 네트워크 동기화가 이루어지는 객체들.
* 클라이언트와 수명 주기를 공유.
* PlayerClient의 ActorNetObserver를 통해 통신.
* 
* Actor 생성 시, 본인을 감시하는 ActorNetObserver를 생성합니다. (생성자)
* 
* @see ActorNetObserver 참고.
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
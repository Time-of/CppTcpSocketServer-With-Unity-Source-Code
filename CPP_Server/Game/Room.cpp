#include "pch.h"

#include "Room.h"

#include "Server/TcpServer.h"
#include "Game/Lobby.h"
#include "Game/Actor/Actor.h"
#include "Game/Actor/ActorNetObserver.h"


Room::Room(const CreateRoomInfo& roomInfo, uint32 guid, std::shared_ptr<Lobby> lobbyPtr) noexcept : 
	roomMasterPlayerId(-1),
	state(ERoomState::CREATED),
	roomName(roomInfo.roomName),
	roomGuid(guid),
	validPlayerCount(0),
	LoadSceneFinishedPlayerCount(0),
	actorMap(),
	lobby(lobbyPtr)
{
	FD_ZERO(&fdMaster);
	FD_SET(GetTcpServer.GetSocket(), &fdMaster);
	maxFd = max(maxFd, GetTcpServer.GetSocket());

	for (int i = 0; i < 4; ++i)
	{
		activePlayers.push_back(nullptr);
	}

	actorMap.reserve(64);
}


Room::~Room()
{
	NotifyRoomShouldBeDestroyed();

	{
		LOCKGUARD(playerControlMutex, playerLock);
		for (auto& player : activePlayers)
		{
			if (player == nullptr) continue;
			player->bIsConnected = false;
			OnPlayerLeaveRoom(player);
		}
	}
	
	roomControlThread.join();
	LOG_INFO(FMT("방 [{}] 소멸!", roomGuid));
}


void Room::Receive()
{
	uint8 outCommand, outOption;
	Byte receivedPacket[CVSPv2::STANDARD_PAYLOAD_LENGTH - sizeof(CVSPv2Header)];

	fd_set fdReadSet, fdErrorSet;
	struct timeval tvs;
	tvs.tv_sec = 0;
	tvs.tv_usec = 100;
	int selectResult;


	LOG_INFO("소켓 메시지 수신 대기 중...");

	while (TcpServer::GetInstance().GetIsRunning() and state != ERoomState::PENDING_DESTROY)
	{
		fdReadSet = fdMaster;
		fdErrorSet = fdMaster;
		selectResult = select(maxFd + 1, &fdReadSet, nullptr, &fdErrorSet, &tvs);
		if (selectResult < 0)
		{
			LOG_ERROR(FMT("select 오류 발생!: {}", selectResult));
			LOG_ERROR(FMT("GetLastError(): {}", GetLastError()));
			continue;
		}


		for (auto& player : activePlayers)
		{
			if (player == nullptr or !FD_ISSET(player->connectSocket, &fdReadSet) or !player->bIsConnected) continue;
			ZeroMemory(receivedPacket, sizeof(receivedPacket));


			int playerId = player->info.playerId;


			int recvLength = CVSPv2::Recv(player->connectSocket, outCommand, outOption, receivedPacket);
			if (recvLength == SOCKET_ERROR or outOption == CVSPv2::Option::Failed)
			{
				LOG_ERROR(FMT("클라이언트 [{}] 에게서 Recv 오류!", playerId));
				LOG_ERROR(FMT("GetLastError(): ", GetLastError()));
				break;
			}
			//LOG_INFO(FMT("[{}]에게서 데이터 수신! Command: {}", playerId, outCommand));


			switch (outCommand)
			{
				// 액터 위치 동기화
				[[likely]]
				case (CVSPv2::Command::SyncActorTransform | CVSPv2::Command::REQUEST):
				{
					uint32 actorGuid = Serializer::PeekUint32(receivedPacket, 1);
					GetThreadPool->AddTask(&Room::_HandleSyncActorTransform,
						this,
						outOption,
						recvLength,
						receivedPacket,
						actorGuid);
					break;
				}


				// 채팅 요청
				case (CVSPv2::Command::Chatting | CVSPv2::Command::REQUEST):
				{
					uint16 chattingLength = uintCast16(recvLength);
					receivedPacket[chattingLength] = '\0';

					LOG_INFO(FMT("채팅 요청, 길이: {}", chattingLength));

					std::string chattingMessage = std::format("[{}] {}: {}\n", player->info.playerId, player->info.nickname, (const char*)receivedPacket);
					LOG_INFO(FMT("{}", chattingMessage));


					for (auto& other : activePlayers)
					{
						if (other == nullptr or !other->bIsConnected or other->connectSocket == SOCKET_ERROR) continue;

						int sendResult = CVSPv2::Send((uint32)other->connectSocket, CVSPv2::GetCommand(CVSPv2::Command::Chatting), CVSPv2::Option::Success, chattingMessage.size(), chattingMessage.data());
						if (sendResult < 0) LOG_ERROR("채팅 전송 실패!");
					}

					break;
				}


				// 액터 스폰 요청
				case (CVSPv2::Command::SpawnActor | CVSPv2::Command::REQUEST):
				{
					LOG_INFO(FMT("방 [{}]: [{}]로부터 액터 스폰 요청!", roomGuid, player->info.playerId));
					Serializer serializer;
					serializer.InterpretSerializedPacket(recvLength, receivedPacket);
					ActorSpawnParams spawnParams;
					serializer >> spawnParams;
					GetThreadPool->AddTask([this, params = std::move(spawnParams)]() mutable -> void
						{
							_HandleSpawnActor(std::move(params));
						});
					break;
				}


				// 액터 파괴 요청
				case (CVSPv2::Command::ActorDetroyed | CVSPv2::Command::REQUEST):
				{
					Serializer serializer;
					serializer.InterpretSerializedPacket(recvLength, receivedPacket);
					uint32 destroyedActorGuid = 0;
					serializer >> destroyedActorGuid;
					LOG_INFO(FMT("방 [{}]: [{}]로부터 액터 [{}] 파괴 통보받음!", roomGuid, player->info.playerId, destroyedActorGuid));

					GetThreadPool->AddTask(&Room::_HandleDestroyActor, this, destroyedActorGuid);

					break;
				}


				// 게임 시작 요청
				case (CVSPv2::Command::StartGame | CVSPv2::Command::REQUEST):
				{
					if (player->info.playerId != roomMasterPlayerId)
					{
						LOG_WARN(FMT("방 [{}]의 게임은 방장만 시작 가능! 방장 : {}", roomGuid, roomMasterPlayerId));
						break;
					}

					if (state == ERoomState::OPENED)
					{
						StartGame();
					}
					else
					{
						LOG_WARN(FMT("방 [{}]의 게임을 시작할 수 없습니다! 방 상태가 OPENED가 아님 : {}", roomGuid, static_cast<int>(state)));
					}
					break;
				}


				// 방 떠나기 요청
				case (CVSPv2::Command::LeaveLobby | CVSPv2::Command::REQUEST):
				{
					LOG_INFO(FMT("[{}]로부터 LeaveLobby 요청 수신!", playerId));
					Leave(player);
					break;
				}


				// 연결 끊기 요청
				case (CVSPv2::Command::Disconnect | CVSPv2::Command::REQUEST):
				{
					LOG_INFO(FMT("[{}]로부터 Disconnect 요청 수신!", playerId));
					player->bIsConnected = false; // @todo 플레이어에서 Leave 수행하도록 주체 변경하기...
					Leave(player);
					break;
				}


				// RPC (매우 간략한 버전... 원래는 대략적으로나마 서버 계산 - 클라이언트에게 전송 할 줄 아는데 구현 시간이 부족..)
				case (CVSPv2::Command::Rpc | CVSPv2::Command::REQUEST):
				{
					// 일단은 무지개 반사
					LOG_INFO(FMT("[{}]로부터 Rpc 요청 수신! 모두에게 그대로 되돌려줌!", playerId));
					if (outOption == RPCTarget::AllClients)
					{
						for (auto& player : activePlayers)
						{
							if (player == nullptr or !player->bIsConnected or player->connectSocket == -1) continue;

							CVSPv2::Send(player->connectSocket, CVSPv2::GetCommand(CVSPv2::Command::Rpc), CVSPv2::Option::Success, recvLength, receivedPacket);
						}
					}

					break;
				}


				// 플레이어 씬 로딩 상황 전달받기
				case (CVSPv2::Command::PlayerHasLoadedGameScene | CVSPv2::Command::REQUEST):
				{
					Serializer serializer;
					serializer.InterpretSerializedPacket(recvLength, receivedPacket);
					LoadSceneCompleteInfo info;
					serializer >> info;

					GetThreadPool->AddTask(&Room::_HandlePlayerFinishLoadScene, this, player, info);

					break;
				}


				default:
				{
					LOG_ERROR(FMT("[{}]에게서 예기치 못한 Command 수신됨! 수신된 Command: {}", playerId, outCommand));
					break;
				}
			}
		}
	}

	LOG_WARN(FMT("방 [{}] 의 Receive() 종료!", roomGuid));

	Sleep(3000);
}


bool Room::Join(PlayerPtr player, bool bForceJoin)
{
	if (player == nullptr)
	{
		LOG_CRITICAL("플레이어가 nullptr인데 방에 Join하려 함!!!");
		return false;
	}

	// 방 상태 검사 (OPENED 또는 CREATED가 아니라면 실패)
	if (!(state == ERoomState::OPENED or state == ERoomState::CREATED))
	{
		LOG_ERROR(FMT("플레이어 [{}] Join 실패! 방의 상태가 열려있지 않음!: {} JoinRoom 실패 메시지 전송!", player->info.playerId, static_cast<int>(state)));
		CVSPv2::Send(player->connectSocket, CVSPv2::GetCommand(CVSPv2::Command::JoinRoom), CVSPv2::Option::Failed);
		return false;
	}
	// 플레이어 상태 검사
	else if (!player->bIsConnected
		or player->connectSocket == SOCKET_ERROR)
	{
		LOG_ERROR(FMT("플레이어 [{}] 방 [{}] Join 실패! 연결이 유효하지 않음!", player->info.playerId, roomGuid));
		return false;
	}
	else if (player->state == EPlayerState::IN_ROOM)
	{
		LOG_ERROR(FMT("플레이어 [{}] 이미 방 [{}] 안에 있음!", player->info.playerId, roomGuid));
		CVSPv2::Send(player->connectSocket, CVSPv2::GetCommand(CVSPv2::Command::JoinRoom), CVSPv2::Option::Failed);
		return false;
	}
	// 플레이어 수 검사
	else if (validPlayerCount >= 4)
	{
		LOG_ERROR(FMT("플레이어 [{}] Join 실패! 이미 방 [{}]에 사람이 꽉 차 있음! (4명)", player->info.playerId, roomGuid));
		CVSPv2::Send(player->connectSocket, CVSPv2::GetCommand(CVSPv2::Command::JoinRoom), CVSPv2::Option::Failed);
		return false;
	}
	

	{
		LOCKGUARD(playerControlMutex, playerLock);


		// 남은 자리 중 빈 자리에 넣기
		int foundIndex = -1; 
		for (int i = 0; i < 4; ++i)
		{
			if (activePlayers[i] == nullptr 
				or !activePlayers[i]->bIsConnected 
				or activePlayers[i]->connectSocket == SOCKET_ERROR
				or activePlayers[i]->info.playerId == -1)
			{
				foundIndex = i;
				activePlayers[i] = player;
				break;
			}
		}
		if (foundIndex <= -1)
		{
			LOG_CRITICAL(FMT("플레이어 [{}]가 방 [{}]에서 빈 자리를 찾지 못했음!!!!", player->info.playerId, roomGuid));
			CVSPv2::Send(player->connectSocket, CVSPv2::GetCommand(CVSPv2::Command::JoinRoom), CVSPv2::Option::Failed);
			return false;
		}
		LOG_DEBUG(FMT("플레이어[{}]가 방 [{}]에서 찾은 자리: [{}]", player->info.playerId, roomGuid, foundIndex));


		OnPlayerJoinedRoom(player, foundIndex);


		if (state == ERoomState::CREATED)
		{
			if (ChangeState(ERoomState::OPENED))
			{
				// 방장 플레이어 id 결정
				roomMasterPlayerId = player->info.playerId;
			}
		}


		// 입장 성공 정보 생성
		std::vector<PlayerInfo> activePlayersInfo;
		activePlayersInfo.reserve(4);
		for (int i = 0; i < 4; ++i)
		{
			activePlayersInfo.push_back(activePlayers[i] != nullptr ? activePlayers[i]->info : PlayerInfo());
		}
		JoinRoomResponseInfo joinInfo(roomGuid, roomMasterPlayerId, roomName, activePlayersInfo);


		// 입장 성공 메시지 직렬화
		Serializer serializer;
		serializer << joinInfo;


		// 방 입장 성공 메시지 전송
		LOG_DEBUG(FMT("[{}]에게 방 [{}] 입장 성공 메시지 전송!", player->info.playerId, roomName));
		CVSPv2::SendSerializedPacket(player->connectSocket,
			CVSPv2::GetCommand(CVSPv2::Command::JoinRoom, true),
			CVSPv2::Option::Success,
			serializer);
	}
	

	// @todo 플레이어에게 응답 받아 연결 상태 체크하기
	// @todo 연결 상태 불량인 경우 퇴장 조치.


	return true;
}


bool Room::Leave(PlayerPtr player)
{
	if (player == nullptr)
	{
		LOG_CRITICAL("플레이어가 nullptr입니다!");
		return false;
	}
	else if (player->state != EPlayerState::IN_ROOM)
	{
		LOG_ERROR(FMT("플레이어 [{}] Leave 실패! 이미 룸에 없습니다! state: {}", player->info.playerId, static_cast<int>(player->state)));
		return false;
	}


	{
		LOCKGUARD(playerControlMutex, playerLock);
		OnPlayerLeaveRoom(player);
	}


	return true;
}


void Room::StartGame()
{
	{
		LOCKGUARD(playerControlMutex, playerLock);
		bool bResult = ChangeState(ERoomState::GAME_STARTED);
		if (!bResult)
		{
			LOG_ERROR(FMT("방 [{}]의 게임을 시작할 수 없음!", roomGuid));
			return;
		}

		for (auto& player : activePlayers)
		{
			if (player == nullptr or !player->bIsConnected or player->connectSocket == -1) continue;

			LOG_DEBUG(FMT("방 [{}]에서 모두에게 게임 시작 완료 메시지 전송!", roomGuid));
			CVSPv2::Send(player->connectSocket,
				CVSPv2::GetCommand(CVSPv2::Command::StartGame),
				CVSPv2::Option::Success);
		}
	}
	
}


void Room::_HandleSpawnActor(ActorSpawnParams&& spawnParams)
{
	LOCKGUARD(actorMutex, spawnActorLock);
	ActorSpawnParams params = std::move(spawnParams);


	// 액터 GUID 할당 (중복 방지)
	uint32 actorGuid = FLGuidGenerator::GenerateGUID();
	auto foundActor = actorMap.find(actorGuid);
	for (uint8 count = 0; foundActor != actorMap.end(); ++count)
	{
		LOG_WARN(FMT("방 [{}]: SpawnActor 중 생성한 액터 GUID [{}]가 중복되어 재시도...", roomGuid, actorGuid));
		actorGuid = FLGuidGenerator::GenerateGUID();
		foundActor = actorMap.find(actorGuid);

		if (count > 10)
		{
			// 아마 여기까지 갈 일은 없을 것...
			LOG_ERROR(FMT("방 [{}]: SpawnActor 중 GUID가 10회나 중복되어, 액터 생성 실패!", roomGuid));
			return;
		}
	}
	params.transform.actorGuid = actorGuid;

	// 액터 생성
	LOG_INFO(FMT("방 [{}]: SpawnActor [{}] 완료! 이름: {}, 소유자: [{}]", roomGuid, actorGuid, params.actorName, params.playerId));

	std::shared_ptr<Actor> spawnedActor = std::make_shared<Actor>();
	actorMap[actorGuid] = spawnedActor;
	spawnedActor->Initialize(std::move(params), shared_from_this());
}


void Room::_HandleDestroyActor(uint32 actorGuid)
{
	std::unordered_map<uint32, std::shared_ptr<Actor>>::iterator foundActor = actorMap.end();
	{
		LOCKGUARD(actorMutex, destroyActorLock_FindPhase);
		foundActor = actorMap.find(actorGuid);

		if (foundActor != actorMap.end()) 
		{
			foundActor->second->DestroyActor();
			actorMap.erase(actorGuid);
			LOG_TRACE(FMT("방 [{}]의 actorMap에서 액터 [{}] 성공적으로 제거됨!", roomGuid, actorGuid));
		}
		else
		{
			LOG_ERROR(FMT("방 [{}]의 actorMap에서 액터 [{}] 제거하지 못함! 찾지 못했음!", roomGuid, actorGuid));
		}
	}
}


void Room::_HandleSyncActorTransform(uint8 option, uint16 packetLength, Byte* packet, uint32 actorGuid)
{
	//LOCKGUARD(actorMutex, handleSyncActorTransformLock);
	auto foundActor = actorMap.find(actorGuid);
	if (foundActor == actorMap.end())
	{
		LOG_ERROR(FMT("액터 [{}]의 위치/회전 동기화 실패! 액터가 없습니다!", actorGuid));
		return;
	}


	switch (option)
	{
		case CVSPv2::Option::SyncActorRotationOnly:
		case CVSPv2::Option::SyncActorOthers:
		case CVSPv2::Option::SyncActorPositionOnly:
		case CVSPv2::Option::Success:
			foundActor->second->observer->NotifySyncActor(packetLength, packet, option);
			break;

		default:
			break;
	}
}


void Room::_HandleSpawnActorObserver(SOCKET playerSocket, TransformInfo&& transformInfo)
{
	LOCKGUARD(actorMutex, spawnActorObserverLock);

	if (actorMap[transformInfo.actorGuid] == nullptr)
	{
		LOG_WARN(FMT("액터 [{}]가 없음에도 [{}] 소켓에게 옵저버를 생성시키려 함!", transformInfo.actorGuid, playerSocket));
		return;
	}
	LOG_TRACE(FMT("[{}] 소켓 클라이언트에게 옵저버 [{}] 생성 지시!", playerSocket, transformInfo.actorGuid));

	Serializer serializer;
	serializer << transformInfo;
	serializer.QuickSendSerializedAndInitialize(playerSocket,
		CVSPv2::GetCommand(CVSPv2::Command::ActorNeedToObserved),
		CVSPv2::Option::Success);
}


void Room::_HandleDestroyActorObserver(SOCKET playerSocket, uint32 actorGuid)
{
	LOG_TRACE(FMT("[{}] 소켓 클라이언트에게서 옵저버 [{}] 소멸 지시!", playerSocket, actorGuid));

	Serializer serializer;
	serializer << actorGuid;
	serializer.QuickSendSerializedAndInitialize(playerSocket,
		CVSPv2::GetCommand(CVSPv2::Command::ActorNeedToObserved),
		CVSPv2::Option::Failed);
}


void Room::AddListenerToNotifyActorObserving(PlayerPtr player, uint32 actorGuid, std::weak_ptr<ActorNetObserver> observer)
{
	if (player == nullptr)
	{
		LOG_CRITICAL("플레이어가 nullptr인데 액터 스폰 알림 delegate에 등록하려 함!");
		return;
	}
	
	player->BeginListenActorObserving(actorGuid, observer);
}


void Room::RemoveListenerToNotifyActorObserving(PlayerPtr player, uint32 actorGuid, std::weak_ptr<ActorNetObserver> observer)
{
	if (player == nullptr)
	{
		LOG_CRITICAL("플레이어가 nullptr인데 액터 스폰 알림 delegate에서 삭제하려 함!");
		return;
	}

	player->RemoveListenActorObserver(actorGuid);
}


void Room::ActorSpawnCompleted(ActorSpawnParams&& spawnParams, std::shared_ptr<Actor> spawnedServerActor)
{
	// @todo 여기서 나중에 관찰 조건 설정하기
	// 일단은 모든 플레이어가 모든 액터의 관찰 대상으로 설정.

	LOCKGUARD(playerControlMutex, addActorListeningLock);
	Serializer serializer;
	serializer << spawnParams;
	auto [length, serializedPacket] = serializer.GetSerializedPacket();

	// 모두에게 액터 생성 지시
	for (auto& player : activePlayers)
	{
		if (player == nullptr or !player->bIsConnected) continue;

		// 리스닝 시작
		AddListenerToNotifyActorObserving(player, spawnedServerActor->guid, spawnedServerActor->observer);

		// 액터 생성 명령 전송
		CVSPv2::SendSerializedPacket(player->connectSocket,
			CVSPv2::GetCommand(CVSPv2::Command::SpawnActor),
			CVSPv2::Option::Success,
			length, serializedPacket);
	}
}


void Room::_HandlePlayerFinishLoadScene(PlayerPtr player, LoadSceneCompleteInfo sceneInfo)
{
	LOCKGUARD(playerControlMutex, handlePlayerFinishLoadSceneMutex);


	// 현재 유효한 플레이어 수 넣기
	sceneInfo.validPlayerCount = validPlayerCount;
	Serializer serializer;


	if (sceneInfo.playerId == -1 or sceneInfo.loadedSceneName != gameSceneName)
	{
		LOG_ERROR(FMT("플레이어[{}] 로부터 Scene 로드 상황을 전달받았으나, 정상이 아님! 불러온 씬 이름: [{}]", sceneInfo.playerId, sceneInfo.loadedSceneName));
		LOG_ERROR(FMT("모든 플레이어에게 [{}] Scene 로드 실패 메시지 전송.", sceneInfo.playerId));
		
		
		// 현재까지 로드된 플레이어 숫자 포함 후 직렬화
		sceneInfo.currentLoadedPlayerCount = LoadSceneFinishedPlayerCount;
		serializer << sceneInfo;


		// 모두에게 전송
		for (auto& eachPlayer : activePlayers)
		{
			if (eachPlayer == nullptr or !eachPlayer->bIsConnected) continue;

			serializer.QuickSendSerializedAndInitialize(eachPlayer->connectSocket,
				CVSPv2::GetCommand(CVSPv2::Command::PlayerHasLoadedGameScene),
				CVSPv2::Option::Failed, true);
		}

		
		// 해당 플레이어 퇴장시킴
		OnPlayerLeaveRoom(player);


		// 퇴장시킨 이후, 퇴장시킨 플레이어를 제외하고 모든 플레이어 로드 완료 시
		if (validPlayerCount != 0 
			and state == ERoomState::GAME_STARTED
			and LoadSceneFinishedPlayerCount == validPlayerCount)
		{
			// 남은 모든 플레이어에게, 모든 플레이어가 로딩이 끝났음을 알림
			for (auto& eachPlayer : activePlayers)
			{
				if (eachPlayer == nullptr 
					or !eachPlayer->bIsConnected
					or eachPlayer->info.playerId == player->info.playerId) continue;

				CVSPv2::Send(eachPlayer->connectSocket,
					CVSPv2::GetCommand(CVSPv2::Command::AllPlayersInRoomHaveFinishedLoading),
					CVSPv2::Option::Success);
			}
		}

		return;
	}

	LOG_INFO(FMT("플레이어[{}] 로부터 Scene 로드 완료 메시지 수신! 불러온 씬 이름: [{}]", sceneInfo.playerId, sceneInfo.loadedSceneName));
	LOG_INFO(FMT("모든 플레이어에게 [{}] Scene 로드 성공 메시지 전송.", sceneInfo.playerId));


	// 현재까지 로드된 플레이어 숫자 포함 후 직렬화
	sceneInfo.currentLoadedPlayerCount = ++LoadSceneFinishedPlayerCount;
	serializer << sceneInfo;


	// 모두에게 전송
	for (auto& eachPlayer : activePlayers)
	{
		if (eachPlayer == nullptr or !eachPlayer->bIsConnected) continue;

		serializer.QuickSendSerializedAndInitialize(eachPlayer->connectSocket,
			CVSPv2::GetCommand(CVSPv2::Command::PlayerHasLoadedGameScene),
			CVSPv2::Option::Success, true);
	}


	// 모든 플레이어가 로딩이 끝났다고 판정될 때
	if (LoadSceneFinishedPlayerCount == validPlayerCount)
	{
		// 모든 플레이어에게, 모든 플레이어가 로딩이 끝났음을 알림
		for (auto& eachPlayer : activePlayers)
		{
			if (eachPlayer == nullptr or !eachPlayer->bIsConnected) continue;

			CVSPv2::Send(eachPlayer->connectSocket,
				CVSPv2::GetCommand(CVSPv2::Command::AllPlayersInRoomHaveFinishedLoading),
				CVSPv2::Option::Success);
		}

		// 보스 스폰...
		ActorSpawnParams bossSpawn;
		bossSpawn.actorName = "BossSkeleton";
		bossSpawn.playerId = roomMasterPlayerId;
		bossSpawn.transform.position.z = 3.5f;

		GetThreadPool->AddTask([this, params = std::move(bossSpawn)]() mutable -> void
			{
				_HandleSpawnActor(std::move(params));
			});
	}
}


void Room::OnPlayerJoinedRoom(PlayerPtr player, int8 playerIndex)
{
	if (player == nullptr)
	{
		LOG_CRITICAL("플레이어가 nullptr입니다!");
		return;
	}

	maxFd = max(maxFd, player->connectSocket);
	FD_SET(player->connectSocket, &fdMaster);
	player->state = EPlayerState::IN_ROOM;
	++validPlayerCount;
	LOG_DEBUG(FMT("플레이어 [{}]가 방 [{}] 에 입장! 플레이어 수: {}", player->info.playerId, roomGuid, validPlayerCount));


	// 유저 정보 갱신 정보 구조체 만들어 직렬화
	RoomUserRefreshedInfo refreshedInfo(static_cast<uint8>(playerIndex), player->info);
	Serializer serializer;
	serializer << refreshedInfo;
	auto [length, serializedPacket] = serializer.GetSerializedPacket();


	// 타 플레이어들에게 유저 정보 갱신 알림
	for (auto& otherUser : activePlayers)
	{
		if (otherUser == nullptr or !otherUser->bIsConnected or otherUser->info.playerId == player->info.playerId) continue;
		
		LOG_DEBUG(FMT("플레이어 [{}]의 방 [{}] 입장 정보를 [{}]에게 전송!", player->info.playerId, roomGuid, otherUser->info.playerId));
		CVSPv2::SendSerializedPacket(otherUser->connectSocket,
			CVSPv2::GetCommand(CVSPv2::Command::RoomUserRefreshed),
			CVSPv2::Option::Success,
			length, serializedPacket);
	}
}


void Room::OnPlayerLeaveRoom(PlayerPtr player)
{
	if (player == nullptr)
	{
		LOG_CRITICAL("플레이어가 nullptr입니다!");
		return;
	}
	else if (!player->bIsConnected)
	{
		LOG_WARN("플레이어 연결이 유효하지 않습니다!");
	}

	FD_CLR(player->connectSocket, &fdMaster);


	player->OnPlayerLeaveRoom();


	// 만약 나간 플레이어가 fd 최댓값이었다면 maxFd를 재정의
	// 나간 플레이어가 activePlayers 중 어느 인덱스인지 찾아, nullptr로 대체.
	int8 foundIndex = -1;
	bool bNeedToCheckMaxFd = maxFd == player->connectSocket;
	if (bNeedToCheckMaxFd)
	{
		maxFd = TcpServer::GetInstance().GetSocket();
	}

	for (int i = 0; i < 4; ++i)
	{
		if (activePlayers[i] != nullptr)
		{
			if (activePlayers[i]->info.playerId == player->info.playerId)
			{
				foundIndex = i;
				activePlayers[i].reset();
			}
			else if (bNeedToCheckMaxFd)
			{
				maxFd = max(maxFd, activePlayers[i]->connectSocket);
			}
		}
	}

	if (bNeedToCheckMaxFd)
		LOG_DEBUG(FMT("플레이어 [{}]가 로비를 떠나, maxFd 재설정! maxFd: [{}], 서버 fd: [{}]", player->info.playerId, maxFd, TcpServer::GetInstance().GetSocket()));
	

	if (foundIndex == -1) LOG_CRITICAL("플레이어가 activePlayers 중 어떤 위치에도 없었습니다!!");


	--validPlayerCount;
	LOG_DEBUG(FMT("플레이어 [{}]가 방 [{}] 에서 떠남!! 플레이어 수: {}", player->info.playerId, roomGuid, validPlayerCount));


	// 플레이어를 로비로 이동
	GetTcpServer.GetTransitionVoid()->EnqueuePlayer(player, nullptr);


	// 방 떠나기 성공
	if (player->connectSocket != SOCKET_ERROR and state != ERoomState::GAME_STARTED)
		CVSPv2::Send(player->connectSocket, CVSPv2::GetCommand(CVSPv2::Command::LeaveLobby), CVSPv2::Option::Success);
	else 
		LOG_CRITICAL(FMT("플레이어 [{}]의 소켓에 문제가 있습니다!", player->info.playerId));


	// 플레이어가 아무도 없으면 방 소멸 대기
	if (validPlayerCount == 0)
	{
		LOG_INFO(FMT("방 [{}]에 플레이어가 아무도 없음!", roomGuid));
		NotifyRoomShouldBeDestroyed();
		return;
	}
	// 남은 플레이어가 있는데 방장이 나간 경우
	else if (roomMasterPlayerId == player->info.playerId)
	{
		// 새 방장 찾기
		int32 newRoomMaster = -1;
		std::string newRoomMasterNickname = "";
		for (int i = 0; i < 4; ++i)
		{
			if (activePlayers[i] == nullptr
				or !activePlayers[i]->bIsConnected 
				or activePlayers[i]->connectSocket == SOCKET_ERROR
				or activePlayers[i]->info.playerId == -1) continue;

			newRoomMaster = activePlayers[i]->info.playerId;
			newRoomMasterNickname = activePlayers[i]->info.nickname;
			break;
		}
		if (newRoomMaster <= -1)
		{
			LOG_CRITICAL(FMT("방 [{}] [{}]의 방장 [{}]이 나가, 방장 플레이어를 변경하고자 하였으나, 남은 플레이어가 ({})명 있다고 판단됨에도 방장으로 할당할 사람이 없음!",
				roomGuid, roomName, player->info.playerId, validPlayerCount));
		}
		else
		{
			LOG_INFO(FMT("방 [{}] [{}]의 방장 [{}]이 나가, 방장 플레이어 [{}]로 변경!", roomGuid, roomName, player->info.playerId, newRoomMaster));
			roomMasterPlayerId = newRoomMaster;


			// 정보 만들기
			PlayerInfo roomMasterInfo(newRoomMaster, newRoomMasterNickname);
			Serializer serializer;
			serializer << roomMasterInfo;
			auto [length, serializedPacket] = serializer.GetSerializedPacket();


			// 방장 변경됨 정보 송신
			for (auto& otherUser : activePlayers)
			{
				if (otherUser == nullptr or !otherUser->bIsConnected or otherUser->info.playerId == player->info.playerId) continue;
				
				LOG_DEBUG(FMT("방 [{}] [{}]의 방장이 [{}]로 넘어갔음을 모두에게 전송!", roomGuid, roomName, newRoomMaster));
				CVSPv2::SendSerializedPacket(otherUser->connectSocket,
					CVSPv2::GetCommand(CVSPv2::Command::RoomUserRefreshed),
					CVSPv2::Option::NewRoomMaster,
					length, serializedPacket);
			}
		}
	}


	// 유저 정보 갱신 정보 구조체 만들어 직렬화 (빈 플레이어 객체로 전송)
	RoomUserRefreshedInfo refreshedInfo(static_cast<uint8>(foundIndex), PlayerInfo());
	Serializer serializer;
	serializer << refreshedInfo;
	auto [length, serializedPacket] = serializer.GetSerializedPacket();


	// 타 플레이어들에게 유저 정보 갱신 알림
	for (auto& otherUser : activePlayers)
	{
		if (otherUser == nullptr or !otherUser->bIsConnected or otherUser->info.playerId == player->info.playerId) continue;
	
		LOG_DEBUG(FMT("플레이어 [{}]의 방 [{}] 퇴장 정보를 [{}]에게 전송!", player->info.playerId, roomGuid, otherUser->info.playerId));
		CVSPv2::SendSerializedPacket(otherUser->connectSocket,
			CVSPv2::GetCommand(CVSPv2::Command::RoomUserRefreshed),
			CVSPv2::Option::Success,
			length, serializedPacket);
	}
}


void Room::NotifyRoomShouldBeDestroyed()
{
	if (state == ERoomState::PENDING_DESTROY)
	{
		LOG_WARN(FMT("방 [{}]이 이미 소멸 대기중이거나 소멸했는데 소멸을 지시하려 함!", roomGuid));
		return;
	}

	ChangeState(ERoomState::PENDING_DESTROY);
}


bool Room::ChangeState(ERoomState newState)
{  
	LOCKGUARD(roomStateMutex, stateChangeLock);

	if (state == newState or state == ERoomState::PENDING_DESTROY) return false;

	switch (state)
	{
		case ERoomState::CREATED:
			roomControlThread = std::thread(&Room::Receive, this);
			break;

		case ERoomState::OPENED:
			break;

		case ERoomState::PENDING_DESTROY:
			break;

		case ERoomState::GAME_STARTED:
			break;
	}

	state = newState;

	switch (state)
	{
		case ERoomState::CREATED:
			break;

		case ERoomState::OPENED:
			break;

		case ERoomState::PENDING_DESTROY:
			LOG_INFO(FMT("방 [{}] [{}] 소멸 대기 중... (PENDING_DESTROY)", roomGuid, roomName));
			lobby->RequestDestroyRoom(roomGuid);
			break;

		case ERoomState::GAME_STARTED:
			LOG_INFO(FMT("방 [{}] [{}] 게임 시작!", roomGuid, roomName));
			break;
	}

	return true;
}

#include "pch.h"

#include "Lobby.h"

#include "DBManager.h"
#include "CVSP/CVSPv2.h"
#include "FunctionLibrary/FLGuidGenerator.h"
#include "Game/Room.h"
#include "Server/TcpServer.h"


#define MAX_PLAYER_COUNT 16


Lobby::Lobby() noexcept
{
	for (int i = 0; i < MAX_PLAYER_COUNT; ++i)
	{
		playerPool.push(std::make_shared<PlayerClient>());
	}

	FD_ZERO(&fdMaster);
	FD_SET(TcpServer::GetInstance().GetSocket(), &fdMaster);
	maxFd = max(maxFd, TcpServer::GetInstance().GetSocket());

	roomMap.reserve(8);
}


Lobby::~Lobby()
{
	{
		LOCKGUARD(roomControlMutex, notifyDestroyRoomsLock);
		for (auto& room : roomMap)
		{
			if (room.second == nullptr) continue;
			room.second->NotifyRoomShouldBeDestroyed();
		}
		roomMap.clear();
	}

	{
		LOCKGUARD(playerControlMutex, playerDisconnectLock);
		for (auto& player : activePlayers)
		{
			if (player == nullptr) continue;
			_HandlePlayerDisconnected(player);
		}
	}
}


void Lobby::Receive()
{
	uint8 outCommand, outOption;
	Byte receivedPacket[CVSPv2::STANDARD_PAYLOAD_LENGTH - sizeof(CVSPv2Header)];

	fd_set fdReadSet, fdErrorSet;
	struct timeval tvs;
	tvs.tv_sec = 0;
	tvs.tv_usec = 100;
	int selectResult;


	LOG_INFO("소켓 메시지 수신 대기 중...");

	LOG_INFO(FMT("server fd: {}", TcpServer::GetInstance().GetSocket()));

	while (TcpServer::GetInstance().GetIsRunning())
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


		if (FD_ISSET(TcpServer::GetInstance().GetSocket(), &fdReadSet))
		{
			SOCKET connectSocket = accept(TcpServer::GetInstance().GetSocket(), nullptr, nullptr);

			LOG_INFO(FMT("accept! {}", connectSocket));

			if (connectSocket > 0)
			{
				PlayerPtr foundPlayer = GetPlayerFromPool();
				if (foundPlayer == nullptr)
				{
					LOG_WARN(FMT("플레이어가 접속하였으나 남은 플레이어 풀이 없어 연결하지 않음!"));
					closesocket(connectSocket);
				}
				else
				{
					LOCKGUARD(playerControlMutex, acceptPlayerLock);
					foundPlayer->bIsConnected = true;
					foundPlayer->connectSocket = connectSocket;
					OnPlayerJoinedLobby(foundPlayer);
					LOG_INFO(FMT("접속한 플레이어 [{}]에게 소켓 배정 완료! {}, maxFd: {}", foundPlayer->info.playerId, foundPlayer->connectSocket, maxFd));
				}
			}
		}

		for (auto& player : activePlayers)
		{
			if (player == nullptr or !FD_ISSET(player->connectSocket, &fdReadSet) or !player->bIsConnected) continue;
			ZeroMemory(receivedPacket, sizeof(receivedPacket));
			

			int recvLength = CVSPv2::Recv(player->connectSocket, outCommand, outOption, receivedPacket);
			if (recvLength == SOCKET_ERROR or outOption == CVSPv2::Option::Failed)
			{
				LOG_ERROR(FMT("클라이언트 [{}] 에게서 Recv 오류! recvLength: {}", player->info.playerId, recvLength));
				break;
			}


			LOG_DEBUG(FMT("[{}]에게서 데이터 수신! Command: {}", player->info.playerId, outCommand));


			switch (outCommand)
			{
				// 채팅 요청
				case (CVSPv2::Command::Chatting | CVSPv2::Command::REQUEST):
				{
					uint16 chattingLength = uintCast16(recvLength);
					receivedPacket[chattingLength] = '\0';


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


				// 방 생성 요청
				case (CVSPv2::Command::CreateRoom | CVSPv2::Command::REQUEST):
				{
					LOG_INFO(FMT("[{}]로부터 CreateRoom 요청 수신!", player->info.playerId));

					Serializer serializer;
					serializer.InterpretSerializedPacket(recvLength, receivedPacket);

					// 역직렬화
					CreateRoomInfo roomInfo;
					serializer >> roomInfo;

					GetThreadPool->AddTask(&Lobby::_HandleCreateRoom, this, player, roomInfo);
					
					break;
				}


				// 방 입장 요청
				case (CVSPv2::Command::JoinRoom | CVSPv2::Command::REQUEST):
				{
					LOG_INFO(FMT("[{}]로부터 JoinRoom 요청 수신!", player->info.playerId));

					Serializer serializer;
					serializer.InterpretSerializedPacket(recvLength, receivedPacket);

					// 역직렬화
					JoinRoomRequestInfo roomInfo;
					serializer >> roomInfo;

					GetThreadPool->AddTask(&Lobby::JoinRoom, this, player, roomInfo.roomGuid);

					break;
				}


				// 방 정보 새로고침 요청
				case (CVSPv2::Command::LoadRoomsInfo | CVSPv2::Command::REQUEST):
				{
					LOG_INFO(FMT("[{}]로부터 LoadRoomsInfo 요청 수신!", player->info.playerId));

					std::vector<RoomInfo> roomsInfo;
					roomsInfo.reserve(roomMap.size());

					for (auto& roomPair : roomMap)
					{
						auto& room = roomPair.second;
						if (room == nullptr or room->GetState() != ERoomState::OPENED)
						{
							continue;
						}

						roomsInfo.push_back(RoomInfo(room->validPlayerCount, room->roomGuid, room->roomName));
					}

					if (roomsInfo.size() != 0)
					{
						Serializer serializer;
						RoomInfoList infoList(roomsInfo.size(), std::move(roomsInfo));
						serializer << infoList;

						CVSPv2::SendSerializedPacket(player->connectSocket, 
							CVSPv2::GetCommand(CVSPv2::Command::LoadRoomsInfo),
							CVSPv2::Option::Success,
							serializer);
						LOG_INFO(FMT("[{}]에게 RoomInfoList 송신! 방 개수: {}", player->info.playerId, infoList.roomCount));
					}
					else
					{
						CVSPv2::Send(player->connectSocket, 
							CVSPv2::GetCommand(CVSPv2::Command::LoadRoomsInfo),
							CVSPv2::Option::Failed);
						LOG_INFO(FMT("[{}]에게 RoomInfoList 송신 실패! 사유: 입장 가능한 방이 없음!", player->info.playerId));
					}

					break;
				}


				// 연결 끊기 요청
				case (CVSPv2::Command::Disconnect | CVSPv2::Command::REQUEST):
				{
					LOG_INFO(FMT("[{}]로부터 Disconnect 요청 수신!", player->info.playerId));
					_HandlePlayerDisconnected(player);
					break;
				}


				// 로그인 요청 (임시) @todo
				case (CVSPv2::Command::LoginToServer | CVSPv2::Command::REQUEST):
				{
					LOG_INFO(FMT("[{}]로부터 Login 요청 수신!", player->info.playerId));

					Serializer serializer;
					serializer.InterpretSerializedPacket(recvLength, receivedPacket);
					PlayerInfo receivedInfo;
					std::string password;
					serializer >> receivedInfo >> password;
					
					bool bLoginSuccessed = DBManager::GetInstance().CheckLogin(receivedInfo.nickname, password);

					if (!bLoginSuccessed)
					{
						LOG_INFO(FMT("[{}] Login 실패!", player->info.playerId));
						CVSPv2::Send(player->connectSocket, CVSPv2::GetCommand(CVSPv2::Command::LoginToServer), CVSPv2::Option::Failed);
						break;
					}
					LOG_INFO(FMT("[{}] Login 성공!", player->info.playerId));

					// 플레이어 이름 적용 후 직렬화
					player->info.nickname = receivedInfo.nickname;
					serializer.Initialize();
					serializer << player->info;
					serializer.QuickSendSerializedAndInitialize(player->connectSocket, CVSPv2::GetCommand(CVSPv2::Command::LoginToServer), CVSPv2::Option::Success);
					
					break;
				}


				default:
				{
					LOG_ERROR(FMT("[{}]에게서 예기치 못한 Command 수신됨! 수신된 Command: {}", player->info.playerId, outCommand));
					break;
				}
			}
			
		}
		
	}

	LOG_WARN("로비의 Receive() 종료됨!");
}


bool Lobby::Join(PlayerPtr player, bool bForceJoin)
{
	if (player == nullptr)
	{
		LOG_CRITICAL("플레이어가 nullptr인데 Lobby에 Join하려 합니다!");
		return false;
	}

	LOG_DEBUG(FMT("플레이어 [{}]가 로비로 Join 시도... 소켓: [{}]", player->info.playerId, player->connectSocket));


	// 연결 끊어진 플레이어에 대한 처리
	if (!player->bIsConnected
		or player->connectSocket == SOCKET_ERROR)
	{
		LOG_INFO(FMT("연결이 끊어진 플레이어 [{}]가 로비에 복귀!", player->info.playerId));
		PushToPlayerPool(player);
		return true;
	}


	// 복귀 요청...
	if (!bForceJoin)
	{
		if (player->state == EPlayerState::IN_LOBBY)
		{
			LOG_ERROR(FMT("플레이어 [{}]가 로비 복귀에 실패!", player->info.playerId));
			LOG_ERROR("로비 복귀 실패 사유: 이미 로비에 있음!");

			return false;
		}
	}
	

	{
		// 복귀
		LOCKGUARD(playerControlMutex, playerForceJoinLobbyLock);
		LOG_INFO(FMT("플레이어 [{}] 로비로 복귀!", player->info.playerId));
		OnPlayerJoinedLobby(player);
		activePlayers.push_back(player);
	}

	// @todo 플레이어에게 신호 보내고, 응답 받아 연결 체크하기 (Transition에 있는 동안 응답이 끊겼을 가능성 체크)


	return true;
}


bool Lobby::Leave(PlayerPtr player)
{
	if (player == nullptr)
	{
		LOG_CRITICAL("플레이어가 nullptr입니다!");
		return false;
	}

	LOG_DEBUG(FMT("플레이어 [{}] 로비를 떠날 시도 중...", player->info.playerId));

	if (player->state != EPlayerState::IN_LOBBY)
	{
		LOG_ERROR(FMT("플레이어 [{}] Leave 실패! 이미 로비에 없습니다! state: {}", player->info.playerId, static_cast<int>(player->state)));
		return false;
	}


	{
		LOCKGUARD(playerControlMutex, playerLock);
		OnPlayerLeaveLobby(player);
	}
	

	return true;
}


PlayerPtr Lobby::GetPlayerFromPool()
{
	LOCKGUARD(playerControlMutex, popLock);
	
	if (playerPool.empty()) return nullptr;

	PlayerPtr outPlayer = nullptr;
	outPlayer = playerPool.top();
	playerPool.pop();

	outPlayer->info.playerId = ++totalPlayerConnectedCount;

	activePlayers.push_back(outPlayer);


	LOG_INFO(FMT("플레이어 풀에서 꺼내 ID [{}] 할당!", outPlayer->info.playerId));
	PrintDebugPlayerPoolSize();

	return outPlayer;
}


void Lobby::PushToPlayerPool(PlayerPtr playerIter)
{
	if (playerIter == nullptr)
	{
		LOG_CRITICAL("[!!!] 풀에 돌려 놓으려는데 플레이어가 nullptr입니다!");
		return;
	}

	LOCKGUARD(playerControlMutex, pushLock);

	int returnedPlayerId = playerIter->info.playerId;

	OnPlayerLeaveLobby(playerIter);

	playerIter->bIsConnected = false;
	playerIter->connectSocket = SOCKET_ERROR;
	playerIter->info = PlayerInfo();

	playerPool.push(playerIter);

	LOG_INFO(FMT("플레이어 [{}] 풀에 반환!", returnedPlayerId));

	PrintDebugPlayerPoolSize();
}


void Lobby::PrintDebugPlayerPoolSize() const
{
	LOG_DEBUG(FMT("현재 Player Pool의 size : {}", playerPool.size()));
}


void Lobby::_HandlePlayerDisconnected(PlayerPtr playerIter)
{
	if (playerIter == nullptr)
	{
		LOG_CRITICAL("플레이어가 nullptr입니다!");
		return;
	}

	GetThreadPool->AddTask(&Lobby::PushToPlayerPool, this, playerIter);
}


uint32 Lobby::CreateRoom(PlayerPtr player, const CreateRoomInfo& roomInfo)
{
	if (player == nullptr)
	{
		LOG_CRITICAL("플레이어가 nullptr입니다!");
		return 0;
	}

	// 예외 처리
	if (roomInfo.roomName.length() < 2 or roomInfo.roomName.length() > 30)
	{
		LOG_WARN(FMT("[{}]가 요청한 CreateRoom은 roomName이 너무 길거나 너무 짧아 취소됨!", player->info.playerId));
		return 0;
	}

	LOG_INFO(FMT("방 {} 생성 시도...", roomInfo.roomName));


	if (!player->bIsConnected or player->connectSocket == SOCKET_ERROR)
	{
		LOG_ERROR(FMT("플레이어 [{}]가 연결되어 있지 않은데 방 생성을 시도함! 소켓: [{}]", player->info.playerId, player->connectSocket));
		return 0;
	}


	// 뮤텍스 획득
	LOCKGUARD(roomControlMutex, createRoomLock);
	LOG_INFO(FMT("방 생성 시작! (생성 요청 플레이어: {})", player->info.playerId));
	

	// 방 만들기
	uint32 roomId = FLGuidGenerator::GenerateGUID();
	auto foundRoom = roomMap.find(roomId);
	for (uint8 count = 0; foundRoom != roomMap.end(); ++count)
	{
		LOG_WARN(FMT("CreateRoom 중 방 번호 [{}]가 중복되어 재시도...", roomId));
		roomId = FLGuidGenerator::GenerateGUID();
		foundRoom = roomMap.find(roomId);

		if (count > 10)
		{
			// 아마 여기까지 갈 일은 없을 것...
			LOG_ERROR("CreateRoom 방 생성 중 GUID가 10회나 중복되어, 방 생성 실패!");
			return 0;
		}
	}

	roomMap[roomId] = std::make_shared<Room>(roomInfo, roomId, shared_from_this());
	LOG_INFO(FMT("방 [{}] {} 생성 성공!", roomId, roomInfo.roomName));


	return roomId;
}


void Lobby::DestroyRoom(uint32 roomId)
{
	LOCKGUARD(roomControlMutex, eraseRoomLock);
	auto foundRoom = roomMap.find(roomId);
	if (foundRoom != roomMap.end())
	{
		if (foundRoom->second != nullptr)
		{
			foundRoom->second->NotifyRoomShouldBeDestroyed();
		}
		else
		{
			LOG_CRITICAL(FMT("[!!!] 방 [{}]은 이미 nullptr이지만 key 값은 남아있었습니다!!! 삭제하였습니다!", roomId));
		}

		roomMap.erase(foundRoom);
		LOG_INFO(FMT("방 [{}] 제거됨!", roomId));
	}
	else
	{
		LOG_WARN(FMT("방 [{}]은 이미 roomMap에서 제거되었는데 제거하려고 했음!", roomId));
	}
}


bool Lobby::JoinRoom(PlayerPtr player, uint32 roomId)
{
	if (player == nullptr)
	{
		LOG_CRITICAL("플레이어가 nullptr입니다!");
		return false;
	}
	else if (!player->bIsConnected or player->connectSocket == SOCKET_ERROR)
	{
		LOG_ERROR(FMT("플레이어 [{}]가 연결되어 있지 않은데 방 [{}] 입장을 시도함! 소켓: [{}]", player->info.playerId, roomId, player->connectSocket));
		return false;
	}

	std::unordered_map<uint32, std::shared_ptr<Room>>::iterator foundRoom = roomMap.end();


	{
		// 플레이어 뮤텍스 획득
		LOCKGUARD(playerControlMutex, joinRoomLock);

		LOG_INFO(FMT("플레이어 [{}]가 방 [{}]에 입장 시도...", player->info.playerId, roomId));


		// 방 유효성 체크
		foundRoom = roomMap.find(roomId);
		if (foundRoom == roomMap.end())
		{
			LOG_ERROR(FMT("방 [{}]를 찾지 못함! [{}]에게 Join 실패 메시지 전송!", roomId, player->info.playerId));
			CVSPv2::Send(player->connectSocket, CVSPv2::GetCommand(CVSPv2::Command::JoinRoom), CVSPv2::Option::Failed);
			return false;
		}


		// 로비 떠난 후, TransitionVOID에 넣기.
		OnPlayerLeaveLobby(player);
	}


	// playerControlLock이 닿지 않는 범위.
	// 찾은 방으로 가도록 지시.
	GetTcpServer.GetTransitionVoid()->EnqueuePlayer(player, foundRoom->second);


	return true;
}


void Lobby::_HandleCreateRoom(PlayerPtr player, CreateRoomInfo roomInfo)
{
	if (player == nullptr)
	{
		LOG_CRITICAL("플레이어가 nullptr입니다!");
		return;
	}

	LOG_DEBUG(FMT("플레이어 [{}]가 방 생성을 요청... 소켓: [{}]", player->info.playerId, player->connectSocket));

	if (!player->bIsConnected or player->connectSocket == SOCKET_ERROR)
	{
		LOG_ERROR(FMT("방 생성 요청한 플레이어 [{}]의 소켓 [{}]이 비정상 상태!", player->info.playerId, player->connectSocket));
		return;
	}


	// 방 생성 및 입장 처리
	uint32 createResult = CreateRoom(player, roomInfo);
	if (createResult != 0)
	{
		bool bJoinResult = JoinRoom(player, createResult);
		if (!bJoinResult)
		{
			LOG_ERROR(FMT("CreateRoom 후 [{}]가 방으로 이동하는 데 실패!", player->info.playerId));
			LOG_ERROR(FMT("따라서 방 [{}] 제거 예정...", createResult));

			RequestDestroyRoom(createResult);

			return;
		}
	}
	else
	{
		LOG_ERROR(FMT("플레이어 [{}]가 방 생성 실패! 실패 메시지를 전송함. 소켓: [{}]", player->info.playerId, player->connectSocket));
		CVSPv2::Send(player->connectSocket, CVSPv2::GetCommand(CVSPv2::Command::CreateRoom), CVSPv2::Option::Failed);
	}
}


void Lobby::RequestDestroyRoom(uint32 roomId)
{
	GetThreadPool->AddTask(&Lobby::DestroyRoom, this, roomId);
}


void Lobby::OnPlayerJoinedLobby(PlayerPtr player)
{
	if (player == nullptr)
	{
		LOG_CRITICAL("플레이어가 nullptr입니다!");
		return;
	}

	maxFd = max(maxFd, player->connectSocket);
	FD_SET(player->connectSocket, &fdMaster);
	player->state = EPlayerState::IN_LOBBY;
	LOG_INFO(FMT("플레이어 [{}]가 로비에 입장! maxFd: [{}]", player->info.playerId, maxFd));
}


void Lobby::OnPlayerLeaveLobby(PlayerPtr player)
{
	if (player == nullptr)
	{
		LOG_CRITICAL("플레이어가 nullptr입니다!");
		return;
	}

	FD_CLR(player->connectSocket, &fdMaster);

	// 만약 나간 플레이어가 fd 최댓값이었다면 maxFd를 재정의
	if (maxFd == player->connectSocket)
	{
		maxFd = TcpServer::GetInstance().GetSocket();

		for (auto& otherPlayer : activePlayers)
		{
			if (otherPlayer == nullptr or otherPlayer->info.playerId == player->info.playerId) continue;
			maxFd = max(maxFd, otherPlayer->connectSocket);
		}
		LOG_DEBUG(FMT("플레이어 [{}]가 로비를 떠나, maxFd 재설정! maxFd: [{}], 서버 fd: [{}]", player->info.playerId, maxFd, TcpServer::GetInstance().GetSocket()));
	}

	player->state = EPlayerState::NONE;
	activePlayers.erase(std::remove(activePlayers.begin(), activePlayers.end(), player), activePlayers.end());
	LOG_INFO(FMT("플레이어 [{}]가 로비를 떠났음!", player->info.playerId));
}

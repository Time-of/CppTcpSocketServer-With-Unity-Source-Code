#pragma once

/**
* Collaborative Virtual Service Protocol (CVSP) v2
* 
* 고급게임서버프로그래밍 수업에서 사용했던 CVSP에서
	이성수(Time-of)가 기능을 추가하고 확장한 버전입니다.
*/


#include "CVSPv2PacketInfos.h"
#include "CVSPv2RPC.h"
#include "Types/MyTypes.h"


#define WIN32 1


#ifdef WIN32
#pragma comment(lib, "ws2_32.lib")
#include <Winsock2.h>
#include <io.h>
#endif

#ifndef WIN32
#include <sys/socket.h>
#include <unistd.h>
#endif


/** CVSPv2 헤더. 4바이트 크기. */
struct alignas(4) CVSPv2Header
{
public:
	uint8 command;
	uint8 option;
	uint16 packetLength;
};


/**
* @author 이성수(Time-of)
* @brief CVSPv2 프로토콜을 위한 데이터 모음 클래스.
* 
* @details CVSP 프로토콜은 호서대학교 고급게임서버프로그래밍 수업에서
			강의 자료로 쓰인 프로토콜입니다.
* @details 강의 자료에 쓰였던 CVSP 프로토콜 기반으로 이성수가 구조를
			수정했습니다.
*/
class CVSPv2
{
private:
	explicit CVSPv2() noexcept = delete;
	explicit CVSPv2(const CVSPv2& other) = delete;
	CVSPv2& operator=(const CVSPv2& other) = delete;


public:
	~CVSPv2() = default;

	/** 헤더 온리 정보 전송 */
	static int Send(int socketFd, uint8 command, uint8 option);

	/** 패킷 전송. */
	static int Send(int socketFd, uint8 command, uint8 option, uint16 packetLength, void* payload);

	/**
	* @brief 패킷 수신.
	* @details outPayload의 최대 크기: CVSPv2::STANDARD_PAYLOAD_LENGTH - sizeof(CVSPv2Header) 바이트.
	* @return 헤더를 제외한 패킷의 사이즈.
	*/
	static int Recv(int socketFd, uint8& outCommand, uint8& outOption, void* outPayload);


	/**
	* @brief Serializer로부터 SerializedPacket을 꺼내어 전송합니다.
	* @details 구조체를 그대로 전송하지 않고, 필요한 정보를 이어붙여 전송합니다. (동적 배열)
	* @param serializer 직렬화된 데이터가 들어있는 Serializer 객체
	*/
	static int SendSerializedPacket(int socketFd, uint8 command, uint8 option, Serializer& serializer);

	// 직접 외부에서 GetSerializedPacket()을 호출하여 전송 (여러 번 전송해야 하는 경우)
	static int SendSerializedPacket(int socketFd, uint8 command, uint8 option, uint16 length, const SerializedPacket& serializedPacket);


public:
	/** -- CVSPv2 Info -- */

	constexpr static const uint8 VER = 0x01;
	constexpr static const uint16 PORT = 8768;
	constexpr static const uint16 STANDARD_PAYLOAD_LENGTH = 4096;


public:
	constexpr static uint8 GetCommand(uint8 command, bool bIsResponse = true)
	{
		return (command | (bIsResponse ? Command::RESPONSE : Command::REQUEST));
	}

	/**
	* @brief -- CVSPv2Header의 Command --
	* @brief 가장 앞 비트는 0이면 REQEUST, 1이면 RESPONSE로 사용됨.
	* @details 따라서, Command가 127을 초과하지 않도록 주의할 것...
	*/
	struct Command
	{
	public:
		/** -- CVSPv2Header의 Command -- */

		constexpr static uint8 JoinRoom = 0x01;
		constexpr static uint8 Chatting = 0x02;
		constexpr static uint8 SyncActorTransform = 0x03;
		constexpr static uint8 SpawnActor = 0x04;
		constexpr static uint8 Rpc = 0x05;
		constexpr static uint8 CreateRoom = 0x06;
		constexpr static uint8 LoadRoomsInfo = 0x07;
		constexpr static uint8 RoomUserRefreshed = 0x08;
		constexpr static uint8 StartGame = 0x09; // 게임 시작 명령. Room의 클라이언트들이 게임 씬으로 넘어가는 데 사용됨
		constexpr static uint8 LeaveLobby = 0x0a;
		constexpr static uint8 LoginToServer = 0x0b;
		constexpr static uint8 ActorNeedToObserved = 0x0c; // 액터가 감시되어야 하는가? (ActorNetObserver 스폰)
		constexpr static uint8 ActorDetroyed = 0x0d;
		constexpr static uint8 PlayerHasLoadedGameScene = 0x0e; // 플레이어가 게임 씬을 불러왔는가? (로딩)
		constexpr static uint8 AllPlayersInRoomHaveFinishedLoading = 0x0f; // 모든 플레이어가 로딩 완료
		constexpr static uint8 Disconnect = 0b01111111;

		
	public:
		/** -- Command의 REQ/RES 비트 플래그 -- */

		constexpr static uint8 REQUEST = 0b00000000;
		constexpr static uint8 RESPONSE = 0b10000000;
	};


public:
	/** -- CVSPv2Header의 Option */
	struct Option
	{
		constexpr static const uint8 Success = 0x01;
		constexpr static const uint8 Failed = 0x02;
		constexpr static const uint8 NewRoomMaster = 0x03; // 새 방장
		constexpr static const uint8 SyncActorPositionOnly = 0x04; // 액터 위치만 동기화
		constexpr static const uint8 SyncActorRotationOnly = 0x05; // 액터 회전만 동기화
		constexpr static const uint8 SyncActorOthers = 0x06; 
	};
};

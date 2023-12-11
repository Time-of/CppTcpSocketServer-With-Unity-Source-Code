#pragma once

/**
* Collaborative Virtual Service Protocol (CVSP) v2
* 
* ��ް��Ӽ������α׷��� �������� ����ߴ� CVSP����
	�̼���(Time-of)�� ����� �߰��ϰ� Ȯ���� �����Դϴ�.
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


/** CVSPv2 ���. 4����Ʈ ũ��. */
struct alignas(4) CVSPv2Header
{
public:
	uint8 command;
	uint8 option;
	uint16 packetLength;
};


/**
* @author �̼���(Time-of)
* @brief CVSPv2 ���������� ���� ������ ���� Ŭ����.
* 
* @details CVSP ���������� ȣ�����б� ��ް��Ӽ������α׷��� ��������
			���� �ڷ�� ���� ���������Դϴ�.
* @details ���� �ڷῡ ������ CVSP �������� ������� �̼����� ������
			�����߽��ϴ�.
*/
class CVSPv2
{
private:
	explicit CVSPv2() noexcept = delete;
	explicit CVSPv2(const CVSPv2& other) = delete;
	CVSPv2& operator=(const CVSPv2& other) = delete;


public:
	~CVSPv2() = default;

	/** ��� �¸� ���� ���� */
	static int Send(int socketFd, uint8 command, uint8 option);

	/** ��Ŷ ����. */
	static int Send(int socketFd, uint8 command, uint8 option, uint16 packetLength, void* payload);

	/**
	* @brief ��Ŷ ����.
	* @details outPayload�� �ִ� ũ��: CVSPv2::STANDARD_PAYLOAD_LENGTH - sizeof(CVSPv2Header) ����Ʈ.
	* @return ����� ������ ��Ŷ�� ������.
	*/
	static int Recv(int socketFd, uint8& outCommand, uint8& outOption, void* outPayload);


	/**
	* @brief Serializer�κ��� SerializedPacket�� ������ �����մϴ�.
	* @details ����ü�� �״�� �������� �ʰ�, �ʿ��� ������ �̾�ٿ� �����մϴ�. (���� �迭)
	* @param serializer ����ȭ�� �����Ͱ� ����ִ� Serializer ��ü
	*/
	static int SendSerializedPacket(int socketFd, uint8 command, uint8 option, Serializer& serializer);

	// ���� �ܺο��� GetSerializedPacket()�� ȣ���Ͽ� ���� (���� �� �����ؾ� �ϴ� ���)
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
	* @brief -- CVSPv2Header�� Command --
	* @brief ���� �� ��Ʈ�� 0�̸� REQEUST, 1�̸� RESPONSE�� ����.
	* @details ����, Command�� 127�� �ʰ����� �ʵ��� ������ ��...
	*/
	struct Command
	{
	public:
		/** -- CVSPv2Header�� Command -- */

		constexpr static uint8 JoinRoom = 0x01;
		constexpr static uint8 Chatting = 0x02;
		constexpr static uint8 SyncActorTransform = 0x03;
		constexpr static uint8 SpawnActor = 0x04;
		constexpr static uint8 Rpc = 0x05;
		constexpr static uint8 CreateRoom = 0x06;
		constexpr static uint8 LoadRoomsInfo = 0x07;
		constexpr static uint8 RoomUserRefreshed = 0x08;
		constexpr static uint8 StartGame = 0x09; // ���� ���� ���. Room�� Ŭ���̾�Ʈ���� ���� ������ �Ѿ�� �� ����
		constexpr static uint8 LeaveLobby = 0x0a;
		constexpr static uint8 LoginToServer = 0x0b;
		constexpr static uint8 ActorNeedToObserved = 0x0c; // ���Ͱ� ���õǾ�� �ϴ°�? (ActorNetObserver ����)
		constexpr static uint8 ActorDetroyed = 0x0d;
		constexpr static uint8 PlayerHasLoadedGameScene = 0x0e; // �÷��̾ ���� ���� �ҷ��Դ°�? (�ε�)
		constexpr static uint8 AllPlayersInRoomHaveFinishedLoading = 0x0f; // ��� �÷��̾ �ε� �Ϸ�
		constexpr static uint8 Disconnect = 0b01111111;

		
	public:
		/** -- Command�� REQ/RES ��Ʈ �÷��� -- */

		constexpr static uint8 REQUEST = 0b00000000;
		constexpr static uint8 RESPONSE = 0b10000000;
	};


public:
	/** -- CVSPv2Header�� Option */
	struct Option
	{
		constexpr static const uint8 Success = 0x01;
		constexpr static const uint8 Failed = 0x02;
		constexpr static const uint8 NewRoomMaster = 0x03; // �� ����
		constexpr static const uint8 SyncActorPositionOnly = 0x04; // ���� ��ġ�� ����ȭ
		constexpr static const uint8 SyncActorRotationOnly = 0x05; // ���� ȸ���� ����ȭ
		constexpr static const uint8 SyncActorOthers = 0x06; 
	};
};

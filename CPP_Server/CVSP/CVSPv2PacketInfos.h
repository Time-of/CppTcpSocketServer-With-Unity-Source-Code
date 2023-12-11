#pragma once

#include <vector>
#include <string>

#include "Serialize/Serializer.h"
//#include "Types/MyTypes.h"
#include "Interface/ISerializable.h"


/**
* @brief CVSPv2로 전달될 수 있는 패킷 정보들.
*/


struct ParamTypes
{
public:
	constexpr static Byte UNDEFINED = 0x00;
	constexpr static Byte Uint8 = 0x01;
	constexpr static Byte Uint16 = 0x02;
	constexpr static Byte Uint32 = 0x03;
	constexpr static Byte Uint64 = 0x04;
	constexpr static Byte Int8 = 0x05;
	constexpr static Byte Int16 = 0x06;
	constexpr static Byte Int32 = 0x07;
	constexpr static Byte Int64 = 0x08;
	constexpr static Byte Bool = 0x09;
	constexpr static Byte Float = 0x0a;
	constexpr static Byte Double = 0x0b;
	constexpr static Byte String = 0x0c;
};


/** 직렬화 완료된 패킷 정보!!! */
struct SerializedPacket
{
public:
	SerializedPacket() : typesCount(0), serializedData(nullptr), serializedDataTypes(nullptr) {}

	uint8 typesCount;
	const Byte* serializedData;
	const Byte* serializedDataTypes;
};


// 유니티와 구조를 맞추기 위한 구조체
struct UnityVector3 : public ISerializable
{
public:
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	void Serialize(Serializer& ser) override;
	void Deserialize(Serializer& ser) override;
};


// 유니티와 구조를 맞추기 위한 구조체
struct UnityQuaternion : public ISerializable
{
public:
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 0.0f;

	void Serialize(Serializer& ser) override;
	void Deserialize(Serializer& ser) override;
};


// 위치, 회전 동기화를 위한 정보
// 반드시 actorGuid가 가장 먼저 직렬화되도록 할 것
struct TransformInfo : public ISerializable
{
public:
	TransformInfo() : actorGuid(0), position(), rotation() {}
	TransformInfo(Serializer& ser) { ser >> actorGuid >> position >> rotation; }

	uint32 actorGuid;
	UnityVector3 position;
	UnityQuaternion rotation;

	void Serialize(Serializer& ser) override;
	void Deserialize(Serializer& ser) override;
};


// 액터의 위치 정보만 동기화하기 위한 구조체
struct ActorPositionInfo : public ISerializable
{
public:
	ActorPositionInfo(Serializer& ser) { ser >> actorGuid >> position; }

	uint32 actorGuid;
	UnityVector3 position;

	void Serialize(Serializer& ser) override;
	void Deserialize(Serializer& ser) override;
};


// 액터의 회전 정보만 동기화하기 위한 구조체
struct ActorRotationInfo : public ISerializable
{
public:
	ActorRotationInfo(Serializer& ser) { ser >> actorGuid >> rotation; }

	uint32 actorGuid;
	UnityQuaternion rotation;

	void Serialize(Serializer& ser) override;
	void Deserialize(Serializer& ser) override;
};


// 액터 스폰 정보
struct ActorSpawnParams : public ISerializable
{
public:
	int32 playerId;
	TransformInfo transform;
	std::string actorName;

	void Serialize(Serializer& ser) override;
	void Deserialize(Serializer& ser) override;
};


/** 플레이어 정보. PlayerClient 클래스가 소유. */
struct PlayerInfo : public ISerializable
{
public:
	PlayerInfo() : playerId(-1), nickname() {}
	PlayerInfo(int32 id, const std::string& _nickname) : playerId(id), nickname(_nickname) {}

	int32 playerId;
	std::string nickname;

	void Serialize(Serializer& ser) override;
	void Deserialize(Serializer& ser) override;
};


// 방 생성 요청 정보
struct CreateRoomInfo : public ISerializable
{
public:
	std::string roomName;

	void Serialize(Serializer& ser) override;
	void Deserialize(Serializer& ser) override;
};


// 방 Join 요청
struct JoinRoomRequestInfo : public ISerializable
{
public:
	uint32 roomGuid;

	void Serialize(Serializer& ser) override;
	void Deserialize(Serializer& ser) override;
};


// 방 Join 응답
struct JoinRoomResponseInfo : public ISerializable
{
public:
	JoinRoomResponseInfo() : roomGuid(0), roomMasterPlayerId(-1), roomName(), players(4, PlayerInfo()) {}
	JoinRoomResponseInfo(uint32 guid, int32 masterPlayerId, const std::string& _roomName, const std::vector<PlayerInfo>& activePlayersInfo) : roomGuid(guid), roomMasterPlayerId(masterPlayerId), roomName(_roomName), players(activePlayersInfo) {}

	uint32 roomGuid;
	int32 roomMasterPlayerId;
	std::string roomName;
	std::vector<PlayerInfo> players;

	void Serialize(Serializer& ser) override;
	void Deserialize(Serializer& ser) override;
};


// 방 정보
struct RoomInfo : public ISerializable
{
public:
	RoomInfo() : currentPlayerCount(0), roomGuid(0), roomName() {}
	RoomInfo(uint8 curPlayers, uint32 guid, const std::string& _roomName) : currentPlayerCount(curPlayers), roomGuid(guid), roomName(_roomName) {}

	uint8 currentPlayerCount;
	uint32 roomGuid;
	std::string roomName;

	void Serialize(Serializer& ser) override;
	void Deserialize(Serializer& ser) override;
};


// 방 정보 목록 (주로 로비에서 방 정보 갱신 용도로 사용)
struct RoomInfoList : public ISerializable
{
public:
	RoomInfoList() : roomCount(0), rooms() {}
	RoomInfoList(uint16 _roomCount, std::vector<RoomInfo>&& _rooms) : roomCount(_roomCount), rooms(std::move(_rooms)) {}

	uint16 roomCount;
	std::vector<RoomInfo> rooms;

	void Serialize(Serializer& ser) override;
	void Deserialize(Serializer& ser) override;
};


// 방의 유저 정보가 갱신되었음을 알림
struct RoomUserRefreshedInfo : public ISerializable
{
public:
	RoomUserRefreshedInfo() : refreshedUserIndex(255), info() {}
	RoomUserRefreshedInfo(uint8 playerIndex, const PlayerInfo& _info) : refreshedUserIndex(playerIndex), info(_info) {}

	// 방의 몇 번째 인덱스?
	uint8 refreshedUserIndex;
	PlayerInfo info;
	

	void Serialize(Serializer& ser) override;
	void Deserialize(Serializer& ser) override;
};


// 씬을 성공적으로 불러왔음! 정보
struct LoadSceneCompleteInfo : public ISerializable
{
public:
	LoadSceneCompleteInfo() : playerId(-1), loadedSceneName() {}
	LoadSceneCompleteInfo(uint8 cur, uint8 allValids, int _playerId, const std::string& _loadedSceneName) :
		currentLoadedPlayerCount(cur), validPlayerCount(allValids),
		playerId(_playerId), loadedSceneName(_loadedSceneName)
		{}

	uint8 currentLoadedPlayerCount = 0;
	uint8 validPlayerCount = 0;
	int playerId;
	std::string loadedSceneName;

	void Serialize(Serializer& ser) override;
	void Deserialize(Serializer& ser) override;
};
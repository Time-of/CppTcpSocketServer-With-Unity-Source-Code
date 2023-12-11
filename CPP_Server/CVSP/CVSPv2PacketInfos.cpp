#include "pch.h"

#include "CVSPv2PacketInfos.h"

#include "Serialize/Serializer.h"


void TransformInfo::Serialize(Serializer& ser)
{
	ser << actorGuid << position << rotation;
}

void TransformInfo::Deserialize(Serializer& ser)
{
	ser >> actorGuid >> position >> rotation;
}

void ActorSpawnParams::Serialize(Serializer& ser)
{
	ser << playerId << transform << actorName;
}

void ActorSpawnParams::Deserialize(Serializer& ser)
{
	ser >> playerId >> transform >> actorName;
}

void PlayerInfo::Serialize(Serializer& ser)
{
	ser << playerId << nickname;
}

void PlayerInfo::Deserialize(Serializer& ser)
{
	ser >> playerId >> nickname;
}

void CreateRoomInfo::Serialize(Serializer& ser)
{
	ser << roomName;
}

void CreateRoomInfo::Deserialize(Serializer& ser)
{
	ser >> roomName;
}

void JoinRoomRequestInfo::Serialize(Serializer& ser)
{
	ser << roomGuid;
}

void JoinRoomRequestInfo::Deserialize(Serializer& ser)
{
	ser >> roomGuid;
}

void JoinRoomResponseInfo::Serialize(Serializer& ser)
{
	ser << roomGuid << roomMasterPlayerId << roomName << players[0] << players[1] << players[2] << players[3];
}

void JoinRoomResponseInfo::Deserialize(Serializer& ser)
{
	ser >> roomGuid >> roomMasterPlayerId >> roomName >> players[0] >> players[1] >> players[2] >> players[3];
}

void UnityQuaternion::Serialize(Serializer& ser)
{
	ser << x << y << z << w;
}

void UnityQuaternion::Deserialize(Serializer& ser)
{
	ser >> x >> y >> z >> w;
}

void UnityVector3::Serialize(Serializer& ser)
{
	ser << x << y << z;
}

void UnityVector3::Deserialize(Serializer& ser)
{
	ser >> x >> y >> z;
}

void RoomInfo::Serialize(Serializer& ser)
{
	ser << currentPlayerCount << roomGuid << roomName;
}

void RoomInfo::Deserialize(Serializer& ser)
{
	ser >> currentPlayerCount >> roomGuid >> roomName;
}

void RoomInfoList::Serialize(Serializer& ser)
{
	ser << roomCount;
	for (int i = 0; i < roomCount; ++i)
	{
		ser << rooms[i];
	}
}

void RoomInfoList::Deserialize(Serializer& ser)
{
	ser >> roomCount;
	for (int i = 0; i < roomCount; ++i)
	{
		ser >> rooms[i];
	}
}

void RoomUserRefreshedInfo::Serialize(Serializer& ser)
{
	ser << refreshedUserIndex << info;
}

void RoomUserRefreshedInfo::Deserialize(Serializer& ser)
{
	ser >> refreshedUserIndex >> info;
}

void ActorPositionInfo::Serialize(Serializer& ser) { ser << actorGuid << position; }

void ActorPositionInfo::Deserialize(Serializer& ser) { ser >> actorGuid >> position; }

void ActorRotationInfo::Serialize(Serializer& ser) { ser << actorGuid << rotation; }

void ActorRotationInfo::Deserialize(Serializer& ser) { ser >> actorGuid >> rotation; }

void LoadSceneCompleteInfo::Serialize(Serializer& ser)
{
	ser << currentLoadedPlayerCount << validPlayerCount << playerId << loadedSceneName;
}

void LoadSceneCompleteInfo::Deserialize(Serializer& ser)
{
	ser >> currentLoadedPlayerCount >> validPlayerCount >> playerId >> loadedSceneName;
}

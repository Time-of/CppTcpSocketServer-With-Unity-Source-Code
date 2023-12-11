using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Runtime.ConstrainedExecution;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using Unity.VisualScripting;
using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;


namespace CVSP
{
	// 직렬화를 위해 필요한 타입 정보들. 굳이 필요 없을 것 같긴 함
	// @todo 삭제 여부 검토
	// 만약 타입 정보만 가지고 원하는 타입 객체를 자동 생성하여 역직렬화를 
	//   편리하게 해 줄 수 있는 기능을 구현할 수 있다면, 삭제할 필요 없을 듯
	public static class ParamTypes
	{
		public const byte UNDEFINED = 0x00;
		public const byte Uint8 = 0x01;
		public const byte Uint16 = 0x02;
		public const byte Uint32 = 0x03;
		public const byte Uint64 = 0x04;
		public const byte Int8 = 0x05;
		public const byte Int16 = 0x06;
		public const byte Int32 = 0x07;
		public const byte Int64 = 0x08;
		public const byte Bool = 0x09;
		public const byte Float = 0x0a;
		public const byte Double = 0x0b;
		public const byte String = 0x0c;
	};


	// 서버와 벡터, 쿼터니언을 주고받기 편리하도록 만든 구조체를 빠르게 만들어내기 위한 클래스
	public static class UnityTypeHelper
	{
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static UnityVector3 GetUnityVector3(this UnityEngine.Vector3 v) => new UnityVector3(v.x, v.y, v.z);

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static UnityQuaternion GetUnityQuaternion(this UnityEngine.Quaternion q) => new UnityQuaternion(q.x, q.y, q.z, q.w);
	}


	// 직렬화된 패킷 정보. 주로 서버와 데이터를 편리하게 주고받기 위해 규약된 프로토콜에 가깝습니다.
	[System.Serializable]
	[StructLayout(LayoutKind.Sequential)]
	public unsafe struct SerializedPacket
	{
		[MarshalAs(UnmanagedType.U1)] public byte typesCount;
		[MarshalAs(UnmanagedType.ByValArray)] public byte[] serializedData;
		[MarshalAs(UnmanagedType.ByValArray)] public byte[] serializedDataTypes;
	}


	// SyncActorWorker에 넘길 패킷 정보
	public struct SyncActorWorkerData
	{
		public byte option;
		public CVSPv2Serializer actorSyncSerializer;
	}


	// 유니티 벡터를 서버와 규격을 맞추기 위해 사용
	[System.Serializable]
	[StructLayout(LayoutKind.Sequential)]
	public unsafe struct UnityVector3 : ISerializable
	{
		[MarshalAs(UnmanagedType.R4)] public float x;
		[MarshalAs(UnmanagedType.R4)] public float y;
		[MarshalAs(UnmanagedType.R4)] public float z;
		public UnityVector3(float _x, float _y, float _z)
		{
			x = _x;
			y = _y;
			z = _z;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public UnityEngine.Vector3 Get() => new UnityEngine.Vector3(x, y, z);

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void Serialize(CVSPv2Serializer ser) => ser.Push(x).Push(y).Push(z);

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void Deserialize(CVSPv2Serializer ser) => ser.Get(out x).Get(out y).Get(out z);
	}


	// 유니티 쿼터니언을 서버와 규격을 맞추기 위해 사용
	[System.Serializable]
	[StructLayout(LayoutKind.Sequential)]
	public unsafe struct UnityQuaternion : ISerializable
	{
		[MarshalAs(UnmanagedType.R4)] public float x;
		[MarshalAs(UnmanagedType.R4)] public float y;
		[MarshalAs(UnmanagedType.R4)] public float z;
		[MarshalAs(UnmanagedType.R4)] public float w;
		public UnityQuaternion(float _x, float _y, float _z, float _w)
		{
			x = _x;
			y = _y;
			z = _z;
			w = _w;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public UnityEngine.Quaternion Get() => new UnityEngine.Quaternion(x, y, z, w);

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void Serialize(CVSPv2Serializer ser) => ser.Push(x).Push(y).Push(z).Push(w);

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void Deserialize(CVSPv2Serializer ser) => ser.Get(out x).Get(out y).Get(out z).Get(out w);
	}


	// 이동 및 회전 정보
	public class TransformInfo : ISerializable
	{
		public TransformInfo()
		{
			actorGuid = 0;
			position = new(0, 0, 0);
			rotation = new(0, 0, 0, 0);
		}

		public TransformInfo(Transform transform)
		{
			actorGuid = 0;
			position = transform.position.GetUnityVector3();
			rotation = transform.rotation.GetUnityQuaternion();
		}

		[MarshalAs(UnmanagedType.U4)]
		public uint actorGuid;
		public UnityVector3 position;
		public UnityQuaternion rotation;

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void Serialize(CVSPv2Serializer ser) => ser.Push(actorGuid).Push(position).Push(rotation);

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void Deserialize(CVSPv2Serializer ser)
		{
			ser.Get(out actorGuid);
			position.Deserialize(ser);
			rotation.Deserialize(ser);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void FromTransformExceptGuid(Transform transform)
		{
			actorGuid = 0;
			position = transform.position.GetUnityVector3();
			rotation = transform.rotation.GetUnityQuaternion();
		}
	}


	/// <summary>
	/// 액터의 위치 정보만 동기화하기 위한 구조체 <br/>
	/// 사실 상, 형식적으로만 존재함. 서버와 규격을 통일하기 위해 존재.
	/// </summary>
	[System.Serializable]
	[StructLayout(LayoutKind.Sequential)]
	public unsafe struct ActorPositionInfo : ISerializable
	{
		[MarshalAs(UnmanagedType.U4)]
		public uint actorGuid;
		public UnityVector3 position;

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void Serialize(CVSPv2Serializer ser) => ser.Push(actorGuid).Push(position);

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void Deserialize(CVSPv2Serializer ser)
		{
			ser.Get(out actorGuid);
			position.Deserialize(ser);
		}
	}


	/// <summary>
	/// 액터의 회전 정보만 동기화하기 위한 구조체 <br/>
	/// 사실 상, 형식적으로만 존재함. 서버와 규격을 통일하기 위해 존재.
	/// </summary>
	[System.Serializable]
	[StructLayout(LayoutKind.Sequential)]
	public unsafe struct ActorRotationInfo : ISerializable
	{
		[MarshalAs(UnmanagedType.U4)]
		public uint actorGuid;
		public UnityQuaternion rotation;

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void Serialize(CVSPv2Serializer ser) => ser.Push(actorGuid).Push(rotation);

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void Deserialize(CVSPv2Serializer ser)
		{
			ser.Get(out actorGuid);
			rotation.Deserialize(ser);
		}
	}


	// 액터 스폰 정보들
	public class ActorSpawnParams : ISerializable
	{
		public int playerId = -1;
		public TransformInfo transform = new();
		public string actorName = "";
		public void Serialize(CVSPv2Serializer ser) => ser.Push(playerId).Push(transform).Push(actorName);
		public void Deserialize(CVSPv2Serializer ser)
		{
			ser.Get(out playerId);
			transform.Deserialize(ser);
			ser.Get(out actorName);
		}
	}


	// 플레이어 정보.
	public class PlayerInfo : ISerializable
	{
		public PlayerInfo()
		{
			playerId = -1;
			nickname = "";
		}
		public PlayerInfo(int playerId, string nickname)
		{
			this.playerId = playerId;
			this.nickname = nickname;
		}
		public int playerId;
		public string nickname;
		public void Serialize(CVSPv2Serializer ser) => ser.Push(playerId).Push(nickname);
		public void Deserialize(CVSPv2Serializer ser) => ser.Get(out playerId).Get(out nickname);
	}


	// 방 생성 정보. 서버로 보낼 것.
	public class CreateRoomInfo : ISerializable
	{
		public CreateRoomInfo(string _roomName) => roomName = _roomName;
		public string roomName;
		public void Serialize(CVSPv2Serializer ser) => ser.Push(roomName);
		public void Deserialize(CVSPv2Serializer ser) => ser.Get(out roomName);
	}


	// 방 입장 요청 정보. 서버로 보낼 것
	public class JoinRoomRequestInfo : ISerializable
	{
		public JoinRoomRequestInfo(uint guid) => roomGuid = guid;
		public uint roomGuid;
		public void Serialize(CVSPv2Serializer ser) => ser.Push(roomGuid);
		public void Deserialize(CVSPv2Serializer ser) => ser.Get(out roomGuid);
	}


	// 방 입장 응답 정보. 방 입장 성공 시 받음.
	public class JoinRoomResponseInfo : ISerializable
	{
		public JoinRoomResponseInfo()
		{
			players = new List<PlayerInfo>(4)
			{
				new PlayerInfo(),
				new PlayerInfo(),
				new PlayerInfo(),
				new PlayerInfo()
			};
		}

		public uint roomGuid = 0;
		public int roomMasterPlayerId = -1;
		public string roomName;
		public List<PlayerInfo> players;

		public void Serialize(CVSPv2Serializer ser) => ser.Push(roomGuid).Push(roomMasterPlayerId).Push(roomName).Push(players[0]).Push(players[1]).Push(players[2]).Push(players[3]);
		public void Deserialize(CVSPv2Serializer ser)
		{
			ser.Get(out roomGuid).Get(out roomMasterPlayerId).Get(out roomName);
			players[0].Deserialize(ser);
			players[1].Deserialize(ser);
			players[2].Deserialize(ser);
			players[3].Deserialize(ser);
		}

	}


	// 방의 정보
	public class RoomInfo : ISerializable
	{
		public RoomInfo()
		{
			currentPlayerCount = 0;
			roomGuid = 0;
			roomName = "";
		}

		public RoomInfo(byte curPlayers, uint guid, string _roomName)
		{
			currentPlayerCount = curPlayers; 
			roomGuid = guid;
			roomName = _roomName;
		}

		public byte currentPlayerCount;
		public uint roomGuid;
		public string roomName;

		public void Serialize(CVSPv2Serializer ser) => ser.Push(currentPlayerCount).Push(roomGuid).Push(roomName);
		public void Deserialize(CVSPv2Serializer ser) => ser.Get(out currentPlayerCount).Get(out roomGuid).Get(out roomName);
	}


	// 방 정보들의 목록 (주로 방 새로고침 시 사용)
	public class RoomInfoList : ISerializable
	{
		public RoomInfoList()
		{
			roomCount = 0;
			rooms = new();
		}
	
		public RoomInfoList(ushort _roomCount, List<RoomInfo> _rooms)
		{
			roomCount = _roomCount;
			rooms = _rooms;
		}

		public ushort roomCount;
		public List<RoomInfo> rooms;

		public void Serialize(CVSPv2Serializer ser)
		{
			ser.Push(roomCount);
			for (int i = 0; i < roomCount; ++i)
			{
				rooms[i].Serialize(ser);
			}
		}
		public void Deserialize(CVSPv2Serializer ser)
		{
			ser.Get(out roomCount);
			rooms = new List<RoomInfo>(roomCount);
			for (int i = 0; i < roomCount; ++i)
			{
				rooms.Add(new RoomInfo());
				rooms[i].Deserialize(ser);
			}
		}
	}


	// 방에서 유저 정보가 갱신되었음을 알리는 정보
	public class RoomUserRefreshedInfo : ISerializable
	{
		public RoomUserRefreshedInfo()
		{
			refreshedUserIndex = 255;
			playerInfo = new PlayerInfo();
		}

		public byte refreshedUserIndex; // 방의 몇 번째 인덱스?
		public PlayerInfo playerInfo;

		public void Serialize(CVSPv2Serializer ser) => ser.Push(refreshedUserIndex).Push(playerInfo);
		public void Deserialize(CVSPv2Serializer ser)
		{
			ser.Get(out refreshedUserIndex);
			playerInfo.Deserialize(ser);
		}
	}


	// 씬을 성공적으로 로드했음! 정보
	public class LoadSceneCompleteInfo : ISerializable
	{
		public LoadSceneCompleteInfo()
		{
			currentLoadedPlayerCount = 0;
			validPlayerCount = 0;
			playerId = -1;
			loadedSceneName = "";
		}

		public LoadSceneCompleteInfo(byte cur, byte allValids, int _playerId, string _roomName)
		{
			currentLoadedPlayerCount = cur;
			validPlayerCount = allValids;
			playerId = _playerId;
			loadedSceneName = _roomName;
		}

		public byte currentLoadedPlayerCount;
		public byte validPlayerCount;
		public int playerId;
		public string loadedSceneName;

		public void Serialize(CVSPv2Serializer ser) => ser.Push(currentLoadedPlayerCount).Push(validPlayerCount).Push(playerId).Push(loadedSceneName);
		public void Deserialize(CVSPv2Serializer ser) => ser.Get(out currentLoadedPlayerCount).Get(out validPlayerCount).Get(out playerId).Get(out loadedSceneName);
	}
}

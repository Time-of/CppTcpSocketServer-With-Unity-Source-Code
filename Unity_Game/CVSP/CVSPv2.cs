using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Linq.Expressions;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Text;
using UnityEngine;
using UnityEngine.Assertions;
using static CVSP.CVSPv2;
using static UnityEngine.Rendering.DebugUI;



/**
* Collaborative Virtual Service Protocol (CVSP) v2
* 
* 고급게임서버프로그래밍 수업에서 사용했던 CVSP에서
	이성수(Time-of)가 기능을 추가하고 확장한 버전입니다.
*/


namespace CVSP
{
	[System.Serializable]
	[StructLayout(LayoutKind.Sequential)]
	public unsafe struct CVSPv2Header
	{
		[MarshalAs(UnmanagedType.U1)] public byte command;
		[MarshalAs(UnmanagedType.U1)] public byte option;
		[MarshalAs(UnmanagedType.U2)] public ushort packetLength;
	}


	/// <summary>
	/// <author>이성수(Time-of)</author>
	/// CVSPv2 프로토콜을 위한 데이터 모음 클래스. <br></br>
	/// 
	/// CVSP 프로토콜은 호서대학교 고급게임서버프로그래밍 수업에서
	///		강의 자료로 쓰인 프로토콜입니다. <br></br>
	///	강의 자료에 쓰였던 CVSP 프로토콜 기반으로 이성수가 구조를
	///		수정했습니다.
	/// </summary>
	public static class CVSPv2
	{
		#region CVSPv2 Info

		public const byte VER = 0x01;
		public const ushort PORT = 8768;
		public const ushort STANDARD_PAYLOAD_LENGTH = 4096;

		#endregion CVSPv2 Info


		#region CVSPv2 Command
		/// <summary>
		/// CVSPv2Header의 Command
		/// 가장 앞 비트를 비트 플래그로 사용
		/// 가장 앞 비트가 0이면 REQUEST, 1이면 RESPONSE
		/// </summary>
		public static class Command
		{
			/** @brief -- CVSPv2Header의 Command -- */

			public const byte JoinRoom = 0x01;
			public const byte Chatting = 0x02;
			public const byte SyncActorTransform = 0x03;
			public const byte SpawnActor = 0x04;
			public const byte Rpc = 0x05;
			public const byte CreateRoom = 0x06;
			public const byte LoadRoomsInfo = 0x07;
			public const byte RoomUserRefreshed = 0x08;
			public const byte StartGame = 0x09;
			public const byte LeaveRoom = 0x0a;
			public const byte LoginToServer = 0x0b;
			public const byte ActorNeedToObserved = 0x0c; // 액터가 감시되어야 하는가? (ActorNetObserver 스폰)
			public const byte ActorDestroyed = 0x0d;
			public const byte PlayerHasLoadedGameScene = 0x0e; // 플레이어가 게임 씬을 불러왔는가? (로딩)
			public const byte AllPlayersInRoomHaveFinishedLoading = 0x0f; // 모든 플레이어가 로딩 완료
			public const byte Disconnect = 0b01111111;


			/** @brief -- Command의 REQ/RES 비트 플래그 -- */

			public const byte REQUEST = 0b00000000;
			public const byte RESPONSE = 0b10000000;
		}
		#endregion


		#region CVSPv2 Option
		/// <summary>
		/// CVSPv2Header의 Option
		/// </summary>
		public static class Option
		{
			public const byte Success = 0x01;
			public const byte Failed = 0x02;
			public const byte NewRoomMaster = 0x03;
			public const byte SyncActorPositionOnly = 0x04; // 액터 위치만 동기화
			public const byte SyncActorRotationOnly = 0x05; // 액터 회전만 동기화
			public const byte SyncActorOthers = 0x06;
			public const byte RequestLogin = 0x07;
		}
		#endregion


		#region 전송(Send)
		/// <summary>
		/// 추가 패킷 없이, 단순 메시지만 전송.
		/// </summary>
		/// <param name="socket">전송할 소켓</param>
		/// <param name="command">명령</param>
		/// <param name="option">옵션</param>
		/// <returns>Send 결과</returns>
		public static int Send(Socket socket, byte command, byte option)
		{
			CVSPv2Header header = new();
			header.command = command;
			header.option = option;
			header.packetLength = (ushort)(4); // 하드코딩, CVSP 구조체의 크기.

			byte[] buffer = new byte[header.packetLength];
			Serializer.StructureToByte(header).CopyTo(buffer, 0);

			return socket.Send(buffer, 0, buffer.Length, SocketFlags.None);
		}


		public static int Send(Socket socket, byte command, byte option, object payload)
		{
			return _InternalSendSerializedBytes(socket, command, option, Serializer.StructureToByte(payload));
		}


		public static int Send(Socket socket, byte command, byte option, byte[] serializedBytes)
		{
			return _InternalSendSerializedBytes(socket, command, option, serializedBytes);
		}


		public static int SendChat(Socket socket, byte command, byte option, string chattingMessage)
		{
			if (chattingMessage.Length == 0)
			{
				Debug.LogWarning("CVSPv2: SendChat 하려는데 보낼 메시지의 길이가 0이라 중단됨!");
				return 0;
			}

			return _InternalSendSerializedBytes(socket, command, option, Serializer.SerializeEurKrString(chattingMessage));
		}


		private static int _InternalSendSerializedBytes(Socket socket, byte command, byte option, byte[] serializedBytes)
		{
			CVSPv2Header header = new();
			header.command = command;
			header.option = option;

			header.packetLength = (ushort)(4 + serializedBytes.Length); // 헤더 크기 하드코딩
			byte[] buffer = new byte[header.packetLength];
			Serializer.StructureToByte(header).CopyTo(buffer, 0);
			serializedBytes.CopyTo(buffer, 4); // 하드코딩, 헤더 뒤에 패킷 붙이기

			return socket.Send(buffer, 0, header.packetLength, SocketFlags.None);
		}


		/// <summary>
		/// CVSPv2Seraializer로부터 SerializedPacket을 꺼내어 전송합니다.
		/// byte로 잘 바꿔서 전송해줍니다. StructureToByte()도 만능은 아니구만...
		/// </summary>
		/// <param name="socket">전송자의 소켓</param>
		/// <param name="command">요청</param>
		/// <param name="option">옵션</param>
		/// <param name="serializer">직렬화된 데이터가 들어있는 CVSPv2Serializer 객체</param>
		/// <returns>전송 결과</returns>
		public static int SendSerializedPacket(Socket socket, byte command, byte option, CVSPv2Serializer serializer)
		{
			(ushort length, SerializedPacket serializedPacket) = serializer.GetSerializedPacket();
			if (length == 0)
			{
				Debug.LogError("SerializedPacket의 length가 0인데 보내려 합니다!!!");
				return -1;
			}


			// 헤더 생성
			CVSPv2Header header = new();
			header.command = command;
			header.option = option;
			header.packetLength = (ushort)(4 + length);


			// 패킷에 헤더 복사
			byte[] packet = new byte[header.packetLength];
			Serializer.StructureToByte(header).CopyTo(packet, 0);


			// 수동 복사 프로토콜......
			packet[4] = serializedPacket.typesCount; // typesCount 복사
			serializedPacket.serializedData.CopyTo(packet, 5); // Data 복사
			serializedPacket.serializedDataTypes.CopyTo(packet, 5 + serializedPacket.serializedData.Length); // DataTypes 복사


			return socket.Send(packet, 0, header.packetLength, SocketFlags.None);
		}


		// 타입에 대해 안전하지 않은 RPC 전송
		// 굉장히 대충대충 구현함....시간이 빠듯해서....
		// 충분한 테스트가 되어있지 않으므로 오작동 가능성 높음
		public static bool SendRpcUnsafe(string functionName, byte target, uint actorGuid, CVSPv2Serializer serializedSerializer)
		{
			int serializedDataCount = serializedSerializer?.serializedData.Count ?? 0;
			ushort length = (ushort)functionName.Length;
			// 4(헤더 크기) + 데이터 개수
			ushort packetLength = (ushort)(10 + serializedDataCount + length);
			byte[] packet = new byte[packetLength];
			packet[0] = Command.Rpc;
			packet[1] = target;
			// packetLength 직렬화
			byte[] packetLengthUnion = new byte[2];
			unsafe
			{
				fixed (byte* ptr = packetLengthUnion)
				{
					*(ushort*)ptr = packetLength;
				}
			}
			packet[2] = packetLengthUnion[0];
			packet[3] = packetLengthUnion[1];
			unsafe
			{
				fixed (byte* ptr = packetLengthUnion)
				{
					*(ushort*)ptr = (ushort)length;
				}
			}
			if (serializedSerializer.bIsPlatformUsingLittleEndian)
			{
				packet[4] = packetLengthUnion[1];
				packet[5] = packetLengthUnion[0];
				Encoding.ASCII.GetBytes(functionName).Reverse().ToArray().CopyTo(packet, 6);
			}
			else
			{
				packet[4] = packetLengthUnion[0];
				packet[5] = packetLengthUnion[1];
				Encoding.ASCII.GetBytes(functionName).CopyTo(packet, 6);
			}

			byte[] actorGuidUnion = new byte[4];
			unsafe
			{
				fixed (byte* ptr = actorGuidUnion)
				{
					*(uint*)ptr = actorGuid;
				}
			}
			if (serializedSerializer.bIsPlatformUsingLittleEndian)
			{
				packet[6 + length] = actorGuidUnion[3];
				packet[7 + length] = actorGuidUnion[2];
				packet[8 + length] = actorGuidUnion[1];
				packet[9 + length] = actorGuidUnion[0];
			}
			else
			{
				packet[6 + length] = actorGuidUnion[0];
				packet[7 + length] = actorGuidUnion[1];
				packet[8 + length] = actorGuidUnion[2];
				packet[9 + length] = actorGuidUnion[3];
			}
			if (serializedSerializer != null) serializedSerializer.serializedData.CopyTo(packet, 10 + length);

			return PlayerClient.instance.GetSocket().Send(packet, 0, packetLength, SocketFlags.None) >= 4;
		}
		#endregion


		#region 받기(Recv)
		public static int Recv(Socket socket, ref byte outCommand, ref byte outOption, ref byte[] outSerializedPacket)
		{
			CVSPv2Header header = new();
			const int headerSize = 4; // 하드코딩
			byte[] headerPacket = new byte[headerSize];
			byte[] receivedPacket = new byte[STANDARD_PAYLOAD_LENGTH];


			// 헤더 패킷만 Peeking하여 가져와, 헤더 존재 여부 검증
			int recvLength = socket.Receive(headerPacket, headerSize, SocketFlags.Peek);
			if (recvLength < 0 || recvLength != 4)
			{
				outOption = Option.Failed;
				Debug.LogWarning("CVSPv2 오류: Recv 중 헤더의 크기가 정상이 아닌 것으로 판단됨!");
				return recvLength;
			}


			// 헤더에 헤더 패킷 복사 후, 남은 패킷 Receive
			header = (CVSPv2Header)Serializer.ByteToStructure(headerPacket, typeof(CVSPv2Header));
			Debug.Assert(header.packetLength <= STANDARD_PAYLOAD_LENGTH);
			int nowRead = 0;
			int sizeNeedToRead = header.packetLength;
			while (nowRead < header.packetLength)
			{
				int result = socket.Receive(receivedPacket, nowRead, sizeNeedToRead, SocketFlags.None);
				if (result < 0)
				{
					Debug.LogWarning("CVSPv2 오류: Recv 중 패킷을 제대로 수신하지 못함!");
					return -1;
				}

				nowRead += result;
				sizeNeedToRead -= result;

				//Debug.Log("CVSPv2: Recv [" + nowRead + " / " + header.packetLength + "]");
			}


			int packetSize = header.packetLength - headerSize;
			outCommand = header.command;
			outOption = header.option;


			// 헤더 이후의 남은 패킷 복사
			if (packetSize != 0)
			{
				Buffer.BlockCopy(receivedPacket, headerSize, outSerializedPacket, 0, packetSize);
			}
			return packetSize;
		}
		#endregion
	}
}

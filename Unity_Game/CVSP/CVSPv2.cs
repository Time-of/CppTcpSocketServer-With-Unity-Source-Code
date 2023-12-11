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
* ��ް��Ӽ������α׷��� �������� ����ߴ� CVSP����
	�̼���(Time-of)�� ����� �߰��ϰ� Ȯ���� �����Դϴ�.
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
	/// <author>�̼���(Time-of)</author>
	/// CVSPv2 ���������� ���� ������ ���� Ŭ����. <br></br>
	/// 
	/// CVSP ���������� ȣ�����б� ��ް��Ӽ������α׷��� ��������
	///		���� �ڷ�� ���� ���������Դϴ�. <br></br>
	///	���� �ڷῡ ������ CVSP �������� ������� �̼����� ������
	///		�����߽��ϴ�.
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
		/// CVSPv2Header�� Command
		/// ���� �� ��Ʈ�� ��Ʈ �÷��׷� ���
		/// ���� �� ��Ʈ�� 0�̸� REQUEST, 1�̸� RESPONSE
		/// </summary>
		public static class Command
		{
			/** @brief -- CVSPv2Header�� Command -- */

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
			public const byte ActorNeedToObserved = 0x0c; // ���Ͱ� ���õǾ�� �ϴ°�? (ActorNetObserver ����)
			public const byte ActorDestroyed = 0x0d;
			public const byte PlayerHasLoadedGameScene = 0x0e; // �÷��̾ ���� ���� �ҷ��Դ°�? (�ε�)
			public const byte AllPlayersInRoomHaveFinishedLoading = 0x0f; // ��� �÷��̾ �ε� �Ϸ�
			public const byte Disconnect = 0b01111111;


			/** @brief -- Command�� REQ/RES ��Ʈ �÷��� -- */

			public const byte REQUEST = 0b00000000;
			public const byte RESPONSE = 0b10000000;
		}
		#endregion


		#region CVSPv2 Option
		/// <summary>
		/// CVSPv2Header�� Option
		/// </summary>
		public static class Option
		{
			public const byte Success = 0x01;
			public const byte Failed = 0x02;
			public const byte NewRoomMaster = 0x03;
			public const byte SyncActorPositionOnly = 0x04; // ���� ��ġ�� ����ȭ
			public const byte SyncActorRotationOnly = 0x05; // ���� ȸ���� ����ȭ
			public const byte SyncActorOthers = 0x06;
			public const byte RequestLogin = 0x07;
		}
		#endregion


		#region ����(Send)
		/// <summary>
		/// �߰� ��Ŷ ����, �ܼ� �޽����� ����.
		/// </summary>
		/// <param name="socket">������ ����</param>
		/// <param name="command">���</param>
		/// <param name="option">�ɼ�</param>
		/// <returns>Send ���</returns>
		public static int Send(Socket socket, byte command, byte option)
		{
			CVSPv2Header header = new();
			header.command = command;
			header.option = option;
			header.packetLength = (ushort)(4); // �ϵ��ڵ�, CVSP ����ü�� ũ��.

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
				Debug.LogWarning("CVSPv2: SendChat �Ϸ��µ� ���� �޽����� ���̰� 0�̶� �ߴܵ�!");
				return 0;
			}

			return _InternalSendSerializedBytes(socket, command, option, Serializer.SerializeEurKrString(chattingMessage));
		}


		private static int _InternalSendSerializedBytes(Socket socket, byte command, byte option, byte[] serializedBytes)
		{
			CVSPv2Header header = new();
			header.command = command;
			header.option = option;

			header.packetLength = (ushort)(4 + serializedBytes.Length); // ��� ũ�� �ϵ��ڵ�
			byte[] buffer = new byte[header.packetLength];
			Serializer.StructureToByte(header).CopyTo(buffer, 0);
			serializedBytes.CopyTo(buffer, 4); // �ϵ��ڵ�, ��� �ڿ� ��Ŷ ���̱�

			return socket.Send(buffer, 0, header.packetLength, SocketFlags.None);
		}


		/// <summary>
		/// CVSPv2Seraializer�κ��� SerializedPacket�� ������ �����մϴ�.
		/// byte�� �� �ٲ㼭 �������ݴϴ�. StructureToByte()�� ������ �ƴϱ���...
		/// </summary>
		/// <param name="socket">�������� ����</param>
		/// <param name="command">��û</param>
		/// <param name="option">�ɼ�</param>
		/// <param name="serializer">����ȭ�� �����Ͱ� ����ִ� CVSPv2Serializer ��ü</param>
		/// <returns>���� ���</returns>
		public static int SendSerializedPacket(Socket socket, byte command, byte option, CVSPv2Serializer serializer)
		{
			(ushort length, SerializedPacket serializedPacket) = serializer.GetSerializedPacket();
			if (length == 0)
			{
				Debug.LogError("SerializedPacket�� length�� 0�ε� ������ �մϴ�!!!");
				return -1;
			}


			// ��� ����
			CVSPv2Header header = new();
			header.command = command;
			header.option = option;
			header.packetLength = (ushort)(4 + length);


			// ��Ŷ�� ��� ����
			byte[] packet = new byte[header.packetLength];
			Serializer.StructureToByte(header).CopyTo(packet, 0);


			// ���� ���� ��������......
			packet[4] = serializedPacket.typesCount; // typesCount ����
			serializedPacket.serializedData.CopyTo(packet, 5); // Data ����
			serializedPacket.serializedDataTypes.CopyTo(packet, 5 + serializedPacket.serializedData.Length); // DataTypes ����


			return socket.Send(packet, 0, header.packetLength, SocketFlags.None);
		}


		// Ÿ�Կ� ���� �������� ���� RPC ����
		// ������ ������� ������....�ð��� �����ؼ�....
		// ����� �׽�Ʈ�� �Ǿ����� �����Ƿ� ���۵� ���ɼ� ����
		public static bool SendRpcUnsafe(string functionName, byte target, uint actorGuid, CVSPv2Serializer serializedSerializer)
		{
			int serializedDataCount = serializedSerializer?.serializedData.Count ?? 0;
			ushort length = (ushort)functionName.Length;
			// 4(��� ũ��) + ������ ����
			ushort packetLength = (ushort)(10 + serializedDataCount + length);
			byte[] packet = new byte[packetLength];
			packet[0] = Command.Rpc;
			packet[1] = target;
			// packetLength ����ȭ
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


		#region �ޱ�(Recv)
		public static int Recv(Socket socket, ref byte outCommand, ref byte outOption, ref byte[] outSerializedPacket)
		{
			CVSPv2Header header = new();
			const int headerSize = 4; // �ϵ��ڵ�
			byte[] headerPacket = new byte[headerSize];
			byte[] receivedPacket = new byte[STANDARD_PAYLOAD_LENGTH];


			// ��� ��Ŷ�� Peeking�Ͽ� ������, ��� ���� ���� ����
			int recvLength = socket.Receive(headerPacket, headerSize, SocketFlags.Peek);
			if (recvLength < 0 || recvLength != 4)
			{
				outOption = Option.Failed;
				Debug.LogWarning("CVSPv2 ����: Recv �� ����� ũ�Ⱑ ������ �ƴ� ������ �Ǵܵ�!");
				return recvLength;
			}


			// ����� ��� ��Ŷ ���� ��, ���� ��Ŷ Receive
			header = (CVSPv2Header)Serializer.ByteToStructure(headerPacket, typeof(CVSPv2Header));
			Debug.Assert(header.packetLength <= STANDARD_PAYLOAD_LENGTH);
			int nowRead = 0;
			int sizeNeedToRead = header.packetLength;
			while (nowRead < header.packetLength)
			{
				int result = socket.Receive(receivedPacket, nowRead, sizeNeedToRead, SocketFlags.None);
				if (result < 0)
				{
					Debug.LogWarning("CVSPv2 ����: Recv �� ��Ŷ�� ����� �������� ����!");
					return -1;
				}

				nowRead += result;
				sizeNeedToRead -= result;

				//Debug.Log("CVSPv2: Recv [" + nowRead + " / " + header.packetLength + "]");
			}


			int packetSize = header.packetLength - headerSize;
			outCommand = header.command;
			outOption = header.option;


			// ��� ������ ���� ��Ŷ ����
			if (packetSize != 0)
			{
				Buffer.BlockCopy(receivedPacket, headerSize, outSerializedPacket, 0, packetSize);
			}
			return packetSize;
		}
		#endregion
	}
}

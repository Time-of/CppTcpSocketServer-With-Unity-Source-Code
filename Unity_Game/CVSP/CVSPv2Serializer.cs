using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Linq;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using static UnityEngine.Rendering.DebugUI;
//using DeserializeIntTemplateLibrary;


namespace CVSP
{
	/// <summary>
	/// 직렬화/역직렬화 클래스.
	/// C++ 서버와 동일한 사양.
	/// 역직렬화 전에, ReadyToGet() 함수 호출 필요.
	/// 직렬화에 넣기: Push()
	/// 역직렬화로 빼기: Get()
	/// 참고로 역직렬화가 완전히 pop하는 개념은 아니기 때문에 Get()이라 네이밍
	/// </summary>
	public class CVSPv2Serializer
	{
		public CVSPv2Serializer()
		{
			Initialize();
			CheckIsPlatformUsingBigEndian();
		}

		public CVSPv2Serializer(int receivedPacketLength, byte[] receivedPacket, bool bReadyToGet)
		{
			currentIndex = 0;
			CheckIsPlatformUsingBigEndian();
			InterpretSerializedPacket(receivedPacketLength, receivedPacket, bReadyToGet);
		}

		public List<byte> serializedData = new();
		public List<byte> serializedDataTypes = new();
		ushort currentIndex = 0;
		public byte[] serializedDataArray = null;

		public bool bIsPlatformUsingLittleEndian = true;


		#region 초기화
		public void Initialize()
		{
			serializedData.Clear();
			serializedDataTypes.Clear();
		
			if (serializedData.Capacity < 128) serializedData.Capacity = 128;
			if (serializedDataTypes.Capacity < 8) serializedDataTypes.Capacity = 8;
			serializedDataArray = null;
			
			currentIndex = 0;
		}


		private void CheckIsPlatformUsingBigEndian()
		{
			bIsPlatformUsingLittleEndian = BitConverter.IsLittleEndian;
		}
		#endregion


		#region 직렬화 및 역직렬화 편의성 기능
		public bool bIsEmpty { get { return serializedData.Count == 0; } }

		/// <summary>
		/// SerializedPacket을 해석합니다.
		/// </summary>
		/// <param name="bReadyToGet">true라면, 배열에 직접 추가합니다. 따라서 ReadyToGet()을 호출할 필요가 없습니다.</param>
		/// <returns>성공 여부</returns>
		public bool InterpretSerializedPacket(int receivedPacketLength, byte[] receivedPacket, bool bReadyToGet)
		{
			if (receivedPacketLength <= 0)
			{
				UnityEngine.Debug.LogError("아무것도 받아오지 못했는데 SerializedPacket을 해석하려 함!");
				return false;
			}
			else if (currentIndex > 0 || serializedData.Count != 0 || serializedDataTypes.Count != 0)
			{
				UnityEngine.Debug.LogWarning("이미 데이터가 있을 확률이 높음에도 SerializedPacket을 해석하려 하고 있습니다!");
				UnityEngine.Debug.LogWarning("Initialize() 이후 호출하거나, 새로 만들어 호출하는 것을 권장합니다.");
				UnityEngine.Debug.LogWarning("currentIndex: " + currentIndex + ", data size: " + serializedData.Count + ", types size: " + serializedDataTypes.Count);
			}


			// 타입 개수 먼저 읽기
			byte typesCount = receivedPacket[0];


			// 크기 추론하기
			ushort serializedDataSize = (ushort)(receivedPacketLength - typesCount - 1);
			


			if (bReadyToGet)
			{
				// 바로 배열로 직빵 복사
				serializedDataArray = receivedPacket.Skip(1).Take(serializedDataSize).ToArray();
			}
			else
			{
				// 복사
				if (serializedData.Capacity < serializedDataSize) serializedData.Capacity = serializedDataSize;
				if (serializedDataTypes.Capacity < typesCount) serializedDataTypes.Capacity = typesCount;
				serializedData.AddRange(receivedPacket.Skip(1).Take(serializedDataSize));
				serializedDataTypes.AddRange(receivedPacket.Skip(1 + serializedDataSize).Take(typesCount));
			}
			

			return true;
		}


		/// <summary>
		/// 빠르게 직렬화해서 바로 전송합니다. <br/>
		/// Send 과정에서 필요한 추가 복사를 생략하기 때문에, 약간 더 빠르게 보낼 수 있습니다. <br/>
		/// Send 후 바로 Initialize()를 호출합니다. <br/>
		/// 참고: header의 packetLength는 리틀/빅 인디언을 구분하지 않음!!!!
		/// <paramref name="bSkipInitialize"/> Initialize() 호출을 생략합니다.
		/// </summary>
		public int QuickSendSerializedAndInitialize(byte command, byte option, bool bSkipInitialize = false)
		{
			if (serializedData.Count <= 0)
			{
				UnityEngine.Debug.LogWarning("데이터가 들어있지 않은데 빠른 전송을 진행하려 함!");
				return -1;
			}
			

			// 4(헤더 크기) + 1 + 데이터 개수 + 타입 개수
			ushort packetLength = (ushort)(5 + serializedData.Count + serializedDataTypes.Count);
			byte[] packet = new byte[packetLength];

			packet[0] = command;
			packet[1] = option;


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
			/* 기존에 사용하던 Marshal.StructureToByte는 리틀 인디언/빅 인디언을 고려하지 않음을 깜빡했음...
			if (bIsPlatformUsingLittleEndian)
			{
				packet[2] = packetLengthUnion[1];
				packet[3] = packetLengthUnion[0];
			}
			else
			{
				packet[2] = packetLengthUnion[0];
				packet[3] = packetLengthUnion[1];
			}
			*/


			packet[4] = (byte)serializedDataTypes.Count; 
			serializedData.CopyTo(packet, 5); 
			serializedDataTypes.CopyTo(packet, 5 + serializedData.Count);


			int result = PlayerClient.instance.GetSocket().Send(packet, 0, packetLength, SocketFlags.None);
			if (!bSkipInitialize) Initialize();
			if (result <= 0) UnityEngine.Debug.LogWarning("QuickSendSerializedAndInitialize(): Send 오류!: " + result);
			return result;
		}


		public (ushort, SerializedPacket) GetSerializedPacket()
		{
			SerializedPacket packet = new();

			if (serializedData.Count == 0 || serializedDataTypes.Count == 0)
			{
				return (0, packet);
			}

			ushort packetSize;

			packet.typesCount = (byte)serializedDataTypes.Count;
			packet.serializedData = serializedData.ToArray();
			packet.serializedDataTypes = serializedDataTypes.ToArray();

			packetSize = (ushort)(1 + serializedData.Count + serializedDataTypes.Count);

			return (packetSize, packet);
		}
		#endregion


		// C# 현 버전에서 시프트 연산자를 지원하지 않음
		public CVSPv2Serializer Push(byte value)
		{
			serializedData.Add(value);
			serializedDataTypes.Add(ParamTypes.Uint8);
			return this;
		}
		public CVSPv2Serializer Push(ushort value)
		{
			byte[] array = new byte[2];

			unsafe
			{
				fixed (byte* ptr = array)
				{
					*(ushort*)ptr = value;
				}
			}

			if (bIsPlatformUsingLittleEndian)
			{
				serializedData.AddRange(array.Reverse());
			}
			else
			{
				serializedData.AddRange(array);
			}

			serializedDataTypes.Add(ParamTypes.Uint16);
			return this;
		}
		public CVSPv2Serializer Push(uint value)
		{
			_Push(BitConverter.GetBytes(value));
			serializedDataTypes.Add(ParamTypes.Uint32);
			return this;
		}
		public CVSPv2Serializer Push(ulong value)
		{
			_Push(BitConverter.GetBytes(value));
			serializedDataTypes.Add(ParamTypes.Uint64);
			return this;
		}
		public CVSPv2Serializer Push(char value)
		{
			_Push(BitConverter.GetBytes(value));
			serializedDataTypes.Add(ParamTypes.Int8);
			return this;
		}
		public CVSPv2Serializer Push(short value)
		{
			_Push(BitConverter.GetBytes(value));
			serializedDataTypes.Add(ParamTypes.Int16);
			return this;
		}
		public CVSPv2Serializer Push(int value)
		{
			_Push(BitConverter.GetBytes(value));
			serializedDataTypes.Add(ParamTypes.Int32);
			return this;
		}
		public CVSPv2Serializer Push(long value)
		{
			_Push(BitConverter.GetBytes(value));
			serializedDataTypes.Add(ParamTypes.Int64);
			return this;
		}
		public CVSPv2Serializer Push(bool value)
		{
			_Push(BitConverter.GetBytes(value));
			serializedDataTypes.Add(ParamTypes.Bool);
			return this;
		}
		public CVSPv2Serializer Push(float value)
		{
			_Push(BitConverter.GetBytes(value));
			serializedDataTypes.Add(ParamTypes.Float);
			return this;
		}
		public CVSPv2Serializer Push(double value)
		{
			_Push(BitConverter.GetBytes(value));
			serializedDataTypes.Add(ParamTypes.Double);
			return this;
		}
		public CVSPv2Serializer Push(string value)
		{
			byte[] array = new byte[2];

			unsafe
			{
				fixed (byte* ptr = array)
				{
					*(ushort*)ptr = (ushort)value.Length;
				}
			}
			if (bIsPlatformUsingLittleEndian)
			{
				serializedData.AddRange(array.Reverse());
			}
			else
			{
				serializedData.AddRange(array);
			}

			_Push(Encoding.ASCII.GetBytes(value));
			serializedDataTypes.Add(ParamTypes.String);
			return this;
		}
		public CVSPv2Serializer Push(ISerializable value)
		{
			value.Serialize(this);
			return this;
		}


		public void PrintDebug()
		{
			string tmp = "";
			for (int i = 0; i < serializedData.Count; ++i)
			{
				tmp += serializedData[i] + " ";
			}

			UnityEngine.Debug.Log("배열: " + tmp);
		}


		public CVSPv2Serializer Get(out byte value)
		{
			value = serializedDataArray[currentIndex];
			currentIndex++;
			return this;
		}
		public CVSPv2Serializer Get(out ushort value)
		{
			if (bIsPlatformUsingLittleEndian)
			{
				value = BitConverter.ToUInt16(_GetReversed(2), 0);
			}
			else
			{
				value = BitConverter.ToUInt16(serializedDataArray, currentIndex);
			}
			currentIndex += 2;
			return this;
		}
		public CVSPv2Serializer Get(out uint value)
		{
			if (bIsPlatformUsingLittleEndian)
			{
				value = BitConverter.ToUInt32(_GetReversed(4), 0);
			}
			else
			{
				value = BitConverter.ToUInt32(serializedDataArray, currentIndex);
			}
			currentIndex += 4;
			return this;
		}
		public CVSPv2Serializer Get(out ulong value)
		{
			if (bIsPlatformUsingLittleEndian)
			{
				value = BitConverter.ToUInt64(_GetReversed(8), 0);
			}
			else
			{
				value = BitConverter.ToUInt64(serializedDataArray, currentIndex);
			}
			currentIndex += 8;
			return this;
		}
		public CVSPv2Serializer Get(out char value)
		{
			value = (char)serializedDataArray[currentIndex];
			currentIndex++;
			return this;
		}
		public CVSPv2Serializer Get(out short value)
		{
			if (bIsPlatformUsingLittleEndian)
			{
				value = BitConverter.ToInt16(_GetReversed(2), 0);
			}
			else
			{
				value = BitConverter.ToInt16(serializedDataArray, currentIndex);
			}
			currentIndex += 2;
			return this;
		}
		public CVSPv2Serializer Get(out int value)
		{
			if (bIsPlatformUsingLittleEndian)
			{
				value = BitConverter.ToInt32(_GetReversed(4), 0);
			}
			else
			{
				value = BitConverter.ToInt32(serializedDataArray, currentIndex);
			}
			currentIndex += 4;
			return this;
		}
		public CVSPv2Serializer Get(out long value)
		{
			if (bIsPlatformUsingLittleEndian)
			{
				value = BitConverter.ToInt64(_GetReversed(8), 0);
			}
			else
			{
				value = BitConverter.ToInt64(serializedDataArray, currentIndex);
			}
			currentIndex += 8;
			return this;
		}
		public CVSPv2Serializer Get(out bool value)
		{
			value = serializedDataArray[currentIndex] != 0;
			currentIndex++;
			return this;
		}
		public CVSPv2Serializer Get(out float value)
		{
			if (bIsPlatformUsingLittleEndian)
			{
				value = BitConverter.ToSingle(_GetReversed(4), 0);
			}
			else
			{
				value = BitConverter.ToSingle(serializedDataArray, currentIndex);
			}
			currentIndex += 4;
			return this;
		}
		public CVSPv2Serializer Get(out double value)
		{
			if (bIsPlatformUsingLittleEndian)
			{
				value = BitConverter.ToDouble(_GetReversed(8), 0);
			}
			else
			{
				value = BitConverter.ToDouble(serializedDataArray, currentIndex);
			}
			currentIndex += 8;
			return this;
		}
		public CVSPv2Serializer Get(out string value)
		{
			Get(out ushort length);
			
			if (bIsPlatformUsingLittleEndian)
			{
				char[] deserializedStr = Encoding.ASCII.GetString(serializedDataArray, currentIndex, length).ToCharArray();
				Array.Reverse(deserializedStr);
				value = new string(deserializedStr);
			}
			else
			{
				value = Encoding.ASCII.GetString(serializedDataArray, currentIndex, length);
			}
			currentIndex += length;
			return this;
		}


		private void _Push(byte[] bytes)
		{
			if (bIsPlatformUsingLittleEndian)
			{
				serializedData.AddRange(bytes.Reverse());
			}
			else
			{
				serializedData.AddRange(bytes);
			}
		}


		private byte[] _GetReversed(int size)
		{
			return serializedDataArray
				.Skip(currentIndex)
				.Take(size)
				.Reverse()
				.ToArray();
		}
	}
}

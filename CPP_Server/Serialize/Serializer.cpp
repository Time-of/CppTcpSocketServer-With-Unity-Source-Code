#include "pch.h"

#include "Serializer.h"

#include "Interface/ISerializable.h"


Serializer::Serializer() noexcept
{
	Initialize();
	CheckIsPlatformUsingBigEndian();
}


Serializer::Serializer(int receivedPacketLength, Byte receivedPacket[]) noexcept : currentIndex(0)
{
	CheckIsPlatformUsingBigEndian();
	InterpretSerializedPacket(receivedPacketLength, receivedPacket);
}


void Serializer::Initialize()
{
	serializedData.clear();
	serializedDataTypes.clear();
	if (serializedData.capacity() < 128) serializedData.reserve(128);
	if (serializedDataTypes.capacity() < 8) serializedDataTypes.reserve(8);

	currentIndex = 0;
}


bool Serializer::InterpretSerializedPacket(int receivedPacketLength, Byte receivedPacket[])
{
	if (receivedPacketLength <= 0)
	{
		LOG_CRITICAL("아무것도 받아오지 못했는데 호출됨!");
		return false;
	}
	else if (currentIndex > 0 or !serializedData.empty() or !serializedDataTypes.empty())
	{
		LOG_WARN("이미 데이터가 있을 확률이 높음에도 SerializedPacket을 해석하려 하고 있습니다!");
		LOG_WARN("Initialize() 이후 호출하거나, 새로 만들어 호출하는 것을 권장합니다.");
		LOG_WARN(FMT("currentIndex: {}, data size: {}, types size: {}", currentIndex, serializedData.size(), serializedDataTypes.size()));
	}


	// 타입 개수 먼저 읽기 (Byte라 그냥 읽어도 됨)
	uint8 typesCount = receivedPacket[0];

	// 크기 추론하기
	const uint16 serializedDataSize = receivedPacketLength - typesCount - 1;

	// 복사
	serializedData.insert(serializedData.end(),
		receivedPacket + 1, receivedPacket + 1 + serializedDataSize);
	serializedDataTypes.insert(serializedDataTypes.end(),
		receivedPacket + 1 + serializedDataSize, receivedPacket + receivedPacketLength);

	return true;
}


int Serializer::QuickSendSerializedAndInitialize(int socketFd, uint8 command, uint8 option, bool bSkipInitialize)
{
	if (serializedData.size() <= 0)
	{
		LOG_WARN("데이터가 들어있지 않은데 빠른 전송을 진행하려 함!");
		return -1;
	}


	// 4(헤더 크기) + 1 + 데이터 개수 + 타입 개수
	// packetLength 직렬화
	union
	{
		uint16 v;
		Byte b[2];
	} packetLengthUnion;
	packetLengthUnion.v = uintCast16(5 + serializedData.size() + serializedDataTypes.size());
	Byte* packet = new Byte[packetLengthUnion.v];


	packet[0] = command;
	packet[1] = option;
	packet[2] = packetLengthUnion.b[0];
	packet[3] = packetLengthUnion.b[1];


	packet[4] = (Byte)serializedDataTypes.size();
	std::copy(serializedData.begin(), serializedData.end(), packet + 5); // serializedData.CopyTo(packet, 5);
	std::copy(serializedDataTypes.begin(), serializedDataTypes.end(), packet + 5 + serializedData.size()); // serializedDataTypes.CopyTo(packet, 5 + serializedData.Count);


	int result = send(socketFd, (const char*)packet, packetLengthUnion.v, 0); // Send(packet, 0, packetLength, SocketFlags.None);
	delete[] packet;
	if (!bSkipInitialize) Initialize();
	if (result <= 0) LOG_WARN(FMT("QuickSendSerializedAndInitialize(): Send 오류!: ", result));
	return result;
}


void Serializer::DebugShowPackets()
{
	printf("\n\n");
	for (int i = 0; i < serializedData.size(); ++i)
	{
		if (i != currentIndex)
			printf("%d ", serializedData[i]);
		else
			printf("[%d] ", serializedData[i]);
	}
	printf("\ncurrentIndex: %d\n\n", currentIndex);
}


uint32 Serializer::PeekUint32(Byte packet[], int startIndex)
{
	uint16 one = 1;
	Byte* testBigEndian = (Byte*)&one;
	bool bIsLittleEndian = testBigEndian[0] == 1;

	if (bIsLittleEndian)
	{
		return (uint32)(packet[startIndex] << 24
			| packet[startIndex + 1] << 16
			| packet[startIndex + 2] << 8
			| packet[startIndex + 3]);
	}
	else
	{
		return (uint32)(packet[startIndex]
			| packet[startIndex + 1] << 8
			| packet[startIndex + 2] << 16
			| packet[startIndex + 3] << 24);
	}
}


const std::tuple<uint16, SerializedPacket> Serializer::GetSerializedPacket() const
{
	SerializedPacket packet;

	if (serializedData.size() == 0 or serializedDataTypes.size() == 0)
	{
		spdlog::error("Serializer: 직렬화된 배열이 없어, SerializedPacket을 가져올 수가 없음!");
		return {0, packet};
	}

	uint16 packetSize = 0;
	
	packet.typesCount = serializedDataTypes.size();
	packet.serializedData = serializedData.data();
	packet.serializedDataTypes = serializedDataTypes.data();

	// typesCount 자료형 크기 + 직렬화된 데이터 크기 + 직렬화된 데이터 타입을 나타내는 상수들의 데이터 크기
	packetSize = uintCast16(sizeof(packet.typesCount)) + serializedData.size() + serializedDataTypes.size();

	return {packetSize, packet};
}


void Serializer::CheckIsPlatformUsingBigEndian()
{
	uint16 one = 1;
	Byte* testBigEndian = (Byte*)&one;
	bIsPlatformUsingLittleEndian = testBigEndian[0] == 1;
}


Serializer& operator<<(Serializer& ser, uint8 value)
{
	ser.serializedData.push_back(value);
	ser.serializedDataTypes.push_back(ParamTypes::Uint8);
	return ser;
}


Serializer& operator<<(Serializer& ser, uint16 value)
{
	ser._SerializeInt<uint16>(value);
	ser.serializedDataTypes.push_back(ParamTypes::Uint16);
	return ser;
}


Serializer& operator<<(Serializer& ser, uint32 value)
{
	ser._SerializeInt<uint32>(value);
	ser.serializedDataTypes.push_back(ParamTypes::Uint32);
	return ser;
}


Serializer& operator<<(Serializer& ser, uint64 value)
{
	ser._SerializeInt<uint64>(value);
	ser.serializedDataTypes.push_back(ParamTypes::Uint64);
	return ser;
}


Serializer& operator<<(Serializer& ser, int8 value)
{
	ser.serializedData.push_back(value);
	ser.serializedDataTypes.push_back(ParamTypes::Int8);
	return ser;
}


Serializer& operator<<(Serializer& ser, int16 value)
{
	ser._SerializeInt<int16>(value);
	ser.serializedDataTypes.push_back(ParamTypes::Int16);
	return ser;
}


Serializer& operator<<(Serializer& ser, int32 value)
{
	ser._SerializeInt<int32>(value);
	ser.serializedDataTypes.push_back(ParamTypes::Int32);
	return ser;
}


Serializer& operator<<(Serializer& ser, int64 value)
{
	ser._SerializeInt<int64>(value);
	ser.serializedDataTypes.push_back(ParamTypes::Int64);
	return ser;
}


Serializer& operator<<(Serializer& ser, bool value)
{
	ser.serializedData.push_back(static_cast<bool>(value));
	ser.serializedDataTypes.push_back(ParamTypes::Bool);
	return ser;
}


Serializer& operator<<(Serializer& ser, float value)
{
	union
	{
		float v;
		Byte b[4];
	} serializeUnion;

	serializeUnion.v = value;

	if (ser.bIsPlatformUsingLittleEndian)
	{
		// 빅 인디언 방식으로 추가
		ser.serializedData.insert(ser.serializedData.end(), std::rbegin(serializeUnion.b), std::rend(serializeUnion.b));
	}
	else
	{
		ser.serializedData.insert(ser.serializedData.end(), std::begin(serializeUnion.b), std::end(serializeUnion.b));
	}

	ser.serializedDataTypes.push_back(ParamTypes::Float);

	return ser;
}


Serializer& operator<<(Serializer& ser, double value)
{
	union
	{
		double v;
		Byte b[8];
	} serializeUnion;

	serializeUnion.v = value;

	if (ser.bIsPlatformUsingLittleEndian)
	{
		// 빅 인디언 방식으로 추가
		ser.serializedData.insert(ser.serializedData.end(), std::rbegin(serializeUnion.b), std::rend(serializeUnion.b));
	}
	else
	{
		ser.serializedData.insert(ser.serializedData.end(), std::begin(serializeUnion.b), std::end(serializeUnion.b));
	}

	ser.serializedDataTypes.push_back(ParamTypes::Double);

	return ser;
}


Serializer& operator<<(Serializer& ser, const std::string& value)
{
	ser._SerializeInt(static_cast<uint16>(value.length()));

	if (ser.bIsPlatformUsingLittleEndian)
	{
		ser.serializedData.insert(ser.serializedData.end(), value.rbegin(), value.rend());
	}
	else
	{
		ser.serializedData.insert(ser.serializedData.end(), value.begin(), value.end());
	}
	ser.serializedDataTypes.push_back(ParamTypes::String);
	return ser;
}


Serializer& operator<<(Serializer& ser, ISerializable& serializable)
{
	// 직렬화 방식 미정의 시 오류
	if (!serializable.bHasOverrided)
	{
		spdlog::error("직렬화 방식이 정의되지 않았음에도 직렬화하려 합니다!!");
		return ser;
	}

	serializable.Serialize(ser);
	
	return ser;
}


Serializer& operator>>(Serializer& ser, uint8& value)
{
	value = ser._DeserializeInt<uint8>();
	return ser;
}


Serializer& operator>>(Serializer& ser, uint16& value)
{
	value = ser._DeserializeInt<uint16>();
	return ser;
}


Serializer& operator>>(Serializer& ser, uint32& value)
{
	value = ser._DeserializeInt<uint32>();
	return ser;
}


Serializer& operator>>(Serializer& ser, uint64& value)
{
	value = ser._DeserializeInt<uint64>();
	return ser;
}


Serializer& operator>>(Serializer& ser, int8& value)
{
	value = ser._DeserializeInt<int8>();
	return ser;
}


Serializer& operator>>(Serializer& ser, int16& value)
{
	value = ser._DeserializeInt<int16>();
	return ser;
}


Serializer& operator>>(Serializer& ser, int32& value)
{
	value = ser._DeserializeInt<int32>();
	return ser;
}


Serializer& operator>>(Serializer& ser, int64& value)
{
	value = ser._DeserializeInt<int64>();
	return ser;
}


Serializer& operator>>(Serializer& ser, bool& value)
{
	value = static_cast<bool>(ser._DeserializeInt<Byte>());
	return ser;
}


Serializer& operator>>(Serializer& ser, float& value)
{
	int32 toInt = ser._DeserializeInt<uint32>();
	value = *(float*)&(toInt);
	return ser;
}


Serializer& operator>>(Serializer& ser, double& value)
{
	int64 toInt = ser._DeserializeInt<uint64>();
	value = *(double*)&(toInt);
	return ser;
}


Serializer& operator>>(Serializer& ser, std::string& value)
{
	uint16 length = ser._DeserializeInt<uint16>();
	if (ser.bIsPlatformUsingLittleEndian)
	{
		std::string deserializedStr(ser.serializedData.begin() + ser.currentIndex, ser.serializedData.begin() + ser.currentIndex + length);
		std::reverse(deserializedStr.begin(), deserializedStr.end());
		value = std::move(deserializedStr);
	}
	else
	{
		value = std::move(std::string(ser.serializedData.begin() + ser.currentIndex, ser.serializedData.begin() + ser.currentIndex + length));
	}
	ser.currentIndex += length;
	return ser;
}


Serializer& operator>>(Serializer& ser, ISerializable& serializable)
{
	// 직렬화 방식 미정의 시 오류
	if (!serializable.bHasOverrided)
	{
		spdlog::error("역직렬화 방식이 정의되지 않았음에도 역직렬화하려 합니다!!");
		return ser;
	}

	serializable.Deserialize(ser);

	return ser;
}

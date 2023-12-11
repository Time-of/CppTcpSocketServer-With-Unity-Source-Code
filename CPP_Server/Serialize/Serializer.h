#pragma once

#include <string>
#include <vector>
#include <type_traits>

#include "Types/MyTypes.h"
#include "spdlog/spdlog.h"


class ISerializable;
class SerializedPacket;


/**
* @author 이성수(Time-of)
* 객체를 직렬화/역직렬화하는 클래스입니다.
*/
class Serializer
{
public:
	explicit Serializer() noexcept;
	explicit Serializer(int receivedPacketLength, Byte receivedPacket[]) noexcept;
	~Serializer() = default;

	void Initialize();

	/**
	* @brief SerializedPacket을 '해석'합니다.
	* @brief 데이터가 없는 상태(ex: 생성 직후)에서 사용하는 것을 추천합니다.
	* @details 데이터가 존재하는 상태에서 사용할 것이라는 가정은 하지 않았음.
	* @details 따라서 데이터가 존재할 때 사용하면 예기치 않은 동작을 수행할 가능성이 있습니다.
	*/
	bool InterpretSerializedPacket(int receivedPacketLength, Byte receivedPacket[]);


	/**
	* @brief 빠르게 직렬화해서 바로 전송합니다.
	* @brief Send 과정에서 필요한 추가 복사를 생략하기 때문에, 약간 더 빠르게 보낼 수 있습니다.
	* 
	* @brief Send 후 바로 Initialize()를 호출합니다.
	* 
	* @details 참고: header의 packetLength는 리틀/빅 인디언을 구분하지 않음!!!!
	* 
	* 
	* @todo GetSerializedPacket()을 개선 가능할 듯. 아예 직렬화된 결과물을 뽑아놓고 여러 플레이어에게 동일 데이터 전송 가능.
	* @param bSkipInitialize true라면 초기화 수행하지 않음
	*/
	int QuickSendSerializedAndInitialize(int socketFd, uint8 command, uint8 option, bool bSkipInitialize = false);


	void DebugShowPackets();

	static uint32 PeekUint32(Byte packet[], int startIndex);


public:
	friend Serializer& operator<<(Serializer& ser, uint8 value);
	friend Serializer& operator<<(Serializer& ser, uint16 value);
	friend Serializer& operator<<(Serializer& ser, uint32 value);
	friend Serializer& operator<<(Serializer& ser, uint64 value);
	friend Serializer& operator<<(Serializer& ser, int8 value);
	friend Serializer& operator<<(Serializer& ser, int16 value);
	friend Serializer& operator<<(Serializer& ser, int32 value);
	friend Serializer& operator<<(Serializer& ser, int64 value);
	friend Serializer& operator<<(Serializer& ser, bool value);
	friend Serializer& operator<<(Serializer& ser, float value);
	friend Serializer& operator<<(Serializer& ser, double value);
	friend Serializer& operator<<(Serializer& ser, const std::string& value);

	friend Serializer& operator<<(Serializer& ser, ISerializable& serializable);

	friend Serializer& operator>>(Serializer& ser, uint8& value);
	friend Serializer& operator>>(Serializer& ser, uint16& value);
	friend Serializer& operator>>(Serializer& ser, uint32& value);
	friend Serializer& operator>>(Serializer& ser, uint64& value);
	friend Serializer& operator>>(Serializer& ser, int8& value);
	friend Serializer& operator>>(Serializer& ser, int16& value);
	friend Serializer& operator>>(Serializer& ser, int32& value);
	friend Serializer& operator>>(Serializer& ser, int64& value);
	friend Serializer& operator>>(Serializer& ser, bool& value);
	friend Serializer& operator>>(Serializer& ser, float& value);
	friend Serializer& operator>>(Serializer& ser, double& value);
	friend Serializer& operator>>(Serializer& ser, std::string& value);

	friend Serializer& operator>>(Serializer& ser, ISerializable& serializable);


public:
	/**
	* @brief 직렬화된 패킷 구조체인 SerializedPacket을 반환합니다.
	* @brief 배열을 반환하므로, 크기 계산을 여기에서 수행하여 함께 반환합니다.
	* @details 패킷 크기가 4092보다 크거나 0이라면 오류가 발생합니다... 현재로서는 4092가 최대 패킷 길이.
	* @details 결국은, '보낼 패킷을 편리하게 관리하고 싶다!'라는 의도에서 만들어진 함수.
	* @return CVSPv2Header에 쓰일 패킷 크기(packetSize)와 SerializedPacket을 반환합니다.
	*/
	const std::tuple<uint16, SerializedPacket> GetSerializedPacket() const;


public:
	std::vector<Byte> serializedData;
	std::vector<Byte> serializedDataTypes;
	uint16 currentIndex = 0;

	bool bIsPlatformUsingLittleEndian = true;


private:
	/** @brief 현재 플랫폼이 빅 인디언을 사용하는지 체크합니다. */
	void CheckIsPlatformUsingBigEndian();


private:
	explicit Serializer(Serializer& other) = delete;
	explicit Serializer(Serializer&& other) noexcept = delete;
	Serializer& operator=(Serializer& other) = delete;
	Serializer& operator=(Serializer&& other) noexcept = delete;


private:
	/**
	* @brief 정수 자료형을 '빅 인디언'으로 직렬화하여 저장합니다.
	* @details 정수형에서만 동작합니다.
	*/
	template<typename IntType>
	void _SerializeInt(IntType value);

	/**
	* @brief '빅 인디언' 형식의 바이트를 정수 자료형으로 역직렬화합니다.
	* @details 정수형에서만 동작합니다.
	*/
	template<typename IntType>
	IntType _DeserializeInt();
};


template<typename IntType>
inline void Serializer::_SerializeInt(IntType value)
{
	static_assert(std::is_integral<IntType>::value);

	// 설마 사이즈가 256 이상이진 않겠지... int자료형인데...
	constexpr uint8 size = uintCast8(sizeof(IntType));

	union
	{
		IntType v;
		Byte b[size];
	} serializeUnion;

	serializeUnion.v = value;

	if (bIsPlatformUsingLittleEndian)
	{
		// 빅 인디언 방식으로 추가
		serializedData.insert(serializedData.end(), std::rbegin(serializeUnion.b), std::rend(serializeUnion.b));
	}
	else
	{
		serializedData.insert(serializedData.end(), std::begin(serializeUnion.b), std::end(serializeUnion.b));
	}
}


template<typename IntType>
inline IntType Serializer::_DeserializeInt()
{
	static_assert(std::is_integral<IntType>::value);

	// 설마 사이즈가 256 이상이진 않겠지... int자료형인데...
	constexpr uint8 size = uintCast8(sizeof(IntType));

	IntType value = 0;

	if (currentIndex + size > serializedData.size())
	{
		spdlog::critical("역직렬화 실패! 타입: {}, 들어온 타입의 크기: {}, serializedData.size(): {}, currentIndex: {}",
			typeid(IntType).name(), size, serializedData.size(), currentIndex);
		spdlog::critical("역직렬화 실패로 인해 0을 반환합니다.");
		return value;
	}

	if (bIsPlatformUsingLittleEndian)
	{
		for (size_t i = 0; i < size; ++i)
		{
			// 빅 인디언을 리틀 인디언으로 해석
			value |= (static_cast<IntType>(serializedData[currentIndex + i]) << (8 * (size - i - 1)));
		}
	}
	else
	{
		for (size_t i = 0; i < size; ++i)
		{
			// 빅 인디언의 자릿수에 맞춰, 그대로 해석
			value |= (static_cast<IntType>(serializedData[currentIndex + i]) << (8 * i));
		}
	}

	currentIndex += size;

	return value;
}

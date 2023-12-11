#pragma once

#include <any>
#include <memory>
#include <string>
#include <vector>

#include "Types/MyTypes.h"
#include "Legacy/Legacy_CVSP.h"


class Legacy_Serializer
{
public:
	// 직렬화할 객체 배열, 직렬화 객체 타입 배열, 직렬화 아웃 배열
	// C#으로 했던 거랑 동일하게 수행하려 했으나, C++은 타입 리플렉션을 지원하지 않음
	// 따라서 타입 정보를 함께 넣어줘야 함...
	// 리틀 인디언 방식!
	// @param objectTypesNotOut objects의 타입 정보를 입력! objects.size() == objectsTypesNotOut.size() 이고 objects.size() < 8이면 objectsTypesNotOut.push_back(RPCValueTypes::UNDEFINED);
	static bool Serialize(const std::vector<std::shared_ptr<std::any>>& objects, std::vector<Byte>& objectTypesNotOut, std::vector<Byte>& outSerializedBytes);

	// @param serializedObjects 유니티에서 받아왔을 때 넣기 편한 형태라 Byte[] 채택
	// @param typeInfos 해석하기 위한 타입 정보
	static std::vector<std::shared_ptr<std::any>> Deserialize(Byte bytesToDeserialize[], Byte typeInfo[8]);


private:
	static int ByteToInt(const std::vector<Byte>& bytes, int startIndex);
	static float ByteToFloat(const std::vector<Byte>& bytes, int startIndex);
	static uint16 ByteToUint16(const std::vector<Byte>& bytes, int startIndex);
	static struct UnityVector3 ByteToVector3(const std::vector<Byte>& bytes, int startIndex);
	static struct UnityQuaternion ByteToQuaternion(const std::vector<Byte>& bytes, int startIndex);


public:
	static std::vector<Byte> StringToByte(const std::string& str);
	static std::string ByteToString(const std::vector<Byte>& bytes, int startIndex, int length);


private:
	explicit Legacy_Serializer() noexcept = delete;
	explicit Legacy_Serializer(Legacy_Serializer&& other) noexcept = delete;
	Legacy_Serializer& operator=(Legacy_Serializer&& other) noexcept = delete;


public:
	~Legacy_Serializer() = default;
};

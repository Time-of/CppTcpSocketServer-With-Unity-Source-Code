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
	// ����ȭ�� ��ü �迭, ����ȭ ��ü Ÿ�� �迭, ����ȭ �ƿ� �迭
	// C#���� �ߴ� �Ŷ� �����ϰ� �����Ϸ� ������, C++�� Ÿ�� ���÷����� �������� ����
	// ���� Ÿ�� ������ �Բ� �־���� ��...
	// ��Ʋ �ε�� ���!
	// @param objectTypesNotOut objects�� Ÿ�� ������ �Է�! objects.size() == objectsTypesNotOut.size() �̰� objects.size() < 8�̸� objectsTypesNotOut.push_back(RPCValueTypes::UNDEFINED);
	static bool Serialize(const std::vector<std::shared_ptr<std::any>>& objects, std::vector<Byte>& objectTypesNotOut, std::vector<Byte>& outSerializedBytes);

	// @param serializedObjects ����Ƽ���� �޾ƿ��� �� �ֱ� ���� ���¶� Byte[] ä��
	// @param typeInfos �ؼ��ϱ� ���� Ÿ�� ����
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

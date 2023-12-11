#include "Legacy_Serializer.h"

#include <iostream>
#include "spdlog/spdlog.h"
#include "CVSP/CVSPv2PacketInfos.h"


bool Legacy_Serializer::Serialize(const std::vector<std::shared_ptr<std::any>>& objects, std::vector<Byte>& objectTypesNotOut, std::vector<Byte>& outSerializedBytes)
{
	if (objects.empty())
	{
		spdlog::error("직렬화 실패: 직렬화 할 파라미터들이 아무것도 없음!");
		return false;
	}
	else if (objects.size() > objectTypesNotOut.size())
	{
		spdlog::error("직렬화 실패: 타입 정보가 부족함!");
		return false;
	}

	if (objectTypesNotOut.size() < 8) objectTypesNotOut.push_back(Legacy_RPCValueType::UNDEFINED);
	
	outSerializedBytes.clear();
	outSerializedBytes.reserve(96);

	union
	{
		uint16 input;
		Byte output[2];
	} serializeUint16;

	union
	{
		int input;
		Byte output[4];
	} serializeInt;

	union
	{
		float input;
		Byte output[4];
	} serializeFloat;

	const int size = objects.size();

	for (int i = 0; i < size; ++i)
	{
		switch (objectTypesNotOut[i])
		{
			case Legacy_RPCValueType::Int:
			{
				serializeInt.input = *std::any_cast<int>(objects[i].get());
				outSerializedBytes.insert(outSerializedBytes.end(), serializeInt.output, serializeInt.output + 4);
				break;
			}

			case Legacy_RPCValueType::Float:
			{
				serializeFloat.input = *std::any_cast<float>(objects[i].get());
				outSerializedBytes.insert(outSerializedBytes.end(), serializeFloat.output, serializeFloat.output + 4);
				break;
			}

			case Legacy_RPCValueType::String:
			{
				std::string str(*std::any_cast<std::string>(objects[i].get()));
				serializeUint16.input = static_cast<uint16>(str.length());
				outSerializedBytes.insert(outSerializedBytes.end(), serializeUint16.output, serializeUint16.output + 2);
				auto strBytes = StringToByte(str);
				outSerializedBytes.insert(outSerializedBytes.end(), strBytes.begin(), strBytes.end());
				break;
			}

			case Legacy_RPCValueType::Vec3:
			{
				UnityVector3 vector3 = *std::any_cast<UnityVector3>(objects[i].get());
				serializeFloat.input = vector3.x;
				outSerializedBytes.insert(outSerializedBytes.end(), serializeFloat.output, serializeFloat.output + 4);
				serializeFloat.input = vector3.y;
				outSerializedBytes.insert(outSerializedBytes.end(), serializeFloat.output, serializeFloat.output + 4);
				serializeFloat.input = vector3.z;
				outSerializedBytes.insert(outSerializedBytes.end(), serializeFloat.output, serializeFloat.output + 4);
				break;
			}

			case Legacy_RPCValueType::Quat:
			{
				UnityQuaternion quat = *std::any_cast<UnityQuaternion>(objects[i].get());
				serializeFloat.input = quat.x;
				outSerializedBytes.insert(outSerializedBytes.end(), serializeFloat.output, serializeFloat.output + 4);
				serializeFloat.input = quat.y;
				outSerializedBytes.insert(outSerializedBytes.end(), serializeFloat.output, serializeFloat.output + 4);
				serializeFloat.input = quat.z;
				outSerializedBytes.insert(outSerializedBytes.end(), serializeFloat.output, serializeFloat.output + 4);
				serializeFloat.input = quat.w;
				outSerializedBytes.insert(outSerializedBytes.end(), serializeFloat.output, serializeFloat.output + 4);
				break;
			}

			default:
			case Legacy_RPCValueType::UNDEFINED:
			{
				spdlog::error("직렬화 실패: 직렬화 중 UNDEFINED나 미정의 타입을 만남!");
				return false;
			}
		}
	}

	spdlog::info("서버에서 객체 직렬화 완료! size: {}", outSerializedBytes.size());

	return true;
}


std::vector<std::shared_ptr<std::any>> Legacy_Serializer::Deserialize(Byte bytesToDeserialize[], Byte typeInfo[8])
{
	int size = static_cast<int>(sizeof(bytesToDeserialize) / sizeof(bytesToDeserialize[0]));
	std::vector<Byte> serializedObjects(bytesToDeserialize, bytesToDeserialize + size);
	size = serializedObjects.size();

	std::vector<std::shared_ptr<std::any>> result;

	int validTypeSize = 0;
	for (int i = 0; i < size; ++i)
	{
		if (typeInfo[i] > Legacy_RPCValueType::UNDEFINED and typeInfo[i] <= Legacy_RPCValueType::Quat)
		{
			++validTypeSize;
		}
		else
		{
			break;
		}
	}
	if (validTypeSize == 0)
	{
		spdlog::error("역직렬화: 데이터가 유효하지 않음!");
		return result;
	}

	result.reserve(validTypeSize);

	spdlog::info("역직렬화 시작! Byte size: {}", size);

	int typeIndex = 0;
	for (int head = 0; head < size and typeIndex < validTypeSize;)
	{
		switch (typeInfo[typeIndex])
		{
			case Legacy_RPCValueType::Int:
			{
				result.push_back(std::make_shared<std::any>(ByteToInt(serializedObjects, head)));
				head += 4;
				break;
			}

			case Legacy_RPCValueType::Float:
			{
				result.push_back(std::make_shared<std::any>(ByteToFloat(serializedObjects, head)));
				head += 4;
				break;
			}

			case Legacy_RPCValueType::String:
			{
				uint16 length = ByteToUint16(serializedObjects, head);
				head += 2;
				result.push_back(std::make_shared<std::any>(ByteToString(serializedObjects, head, length)));
				head += length;
				break;
			}

			case Legacy_RPCValueType::Vec3:
			{
				result.push_back(std::make_shared<std::any>(ByteToVector3(serializedObjects, head)));
				head += 12;
				break;
			}

			case Legacy_RPCValueType::Quat:
			{
				result.push_back(std::make_shared<std::any>(ByteToQuaternion(serializedObjects, head)));
				head += 16;
				break;
			}

			default:
			{
				spdlog::error("역직렬화 실패: 역직렬화 중 UNDEFINED나 미정의 타입을 만남!");
				std::vector<std::shared_ptr<std::any>> tmp;
				return tmp;
			}

			case Legacy_RPCValueType::UNDEFINED:
			{
				spdlog::error("역직렬화 중 UNDEFINED를 만나 바로 return됨!");
				return result;
			}
		}

		++typeIndex;
	}

	spdlog::info("역직렬화 종료!");

	return result;
}


int Legacy_Serializer::ByteToInt(const std::vector<Byte>& bytes, int startIndex)
{
	return bytes[startIndex] | (bytes[startIndex + 1] << 8) | (bytes[startIndex + 2] << 16) | (bytes[startIndex + 3] << 24);
}


float Legacy_Serializer::ByteToFloat(const std::vector<Byte>& bytes, int startIndex)
{
	int toInt = ByteToInt(bytes, startIndex);
	return *(float*)&toInt;
}


uint16 Legacy_Serializer::ByteToUint16(const std::vector<Byte>& bytes, int startIndex)
{
	return static_cast<uint16>(bytes[startIndex] | (bytes[startIndex + 1] << 8));
}


UnityVector3 Legacy_Serializer::ByteToVector3(const std::vector<Byte>& bytes, int startIndex)
{
	UnityVector3 vector3;
	vector3.x = ByteToFloat(bytes, startIndex);
	vector3.y = ByteToFloat(bytes, startIndex + 4);
	vector3.z = ByteToFloat(bytes, startIndex + 8);

	return vector3;
}


UnityQuaternion Legacy_Serializer::ByteToQuaternion(const std::vector<Byte>& bytes, int startIndex)
{
	UnityQuaternion quat;
	quat.x = ByteToFloat(bytes, startIndex);
	quat.y = ByteToFloat(bytes, startIndex + 4);
	quat.z = ByteToFloat(bytes, startIndex + 8);
	quat.w = ByteToFloat(bytes, startIndex + 12);

	return quat;
}


std::vector<Byte> Legacy_Serializer::StringToByte(const std::string& str)
{
	return std::vector<Byte>(str.begin(), str.begin() + str.length());
}


std::string Legacy_Serializer::ByteToString(const std::vector<Byte>& bytes, int startIndex, int length)
{
	return std::string(bytes.begin() + startIndex, bytes.begin() + startIndex + length);
}

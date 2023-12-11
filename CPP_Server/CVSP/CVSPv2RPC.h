#pragma once

#include <string>
#include <vector>

#include "Types/MyTypes.h"
#include "CVSP/CVSPv2PacketInfos.h"


// @todo RPC 구현.
struct RPCInfo
{
public:
	uint32 actorGuid;
	std::string functionName;
};


struct RPCTarget
{
public:
	static const constexpr uint8 UNDEFINED = 0x00;
	static const constexpr uint8 AllClients = 0x01; // 모든 클라이언트에게 전송 (via Server)
	static const constexpr uint8 AllClientsExceptMe = 0x02; // 자신을 제외한 모든 클라이언트에게 전송 (via Server)
	static const constexpr uint8 Server = 0x03; // 서버에만 전송
	static const constexpr uint8 Client = 0x04; // 단일 클라이언트에 전송 (via Server)
};
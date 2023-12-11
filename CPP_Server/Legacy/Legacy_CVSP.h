#pragma once

#include <cassert>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <malloc.h>
#include <string>


// CVSPv2와 중복
#ifndef WIN32
#define WIN32 1
#endif


#ifdef WIN32
#pragma comment(lib, "ws2_32.lib")
#include <winsock.h>
#include <io.h>
#endif

#ifndef WIN32
#include <sys/socket.h>
#include <unistd.h>
#endif

#define CVSP_MONITORING_MESSAGE 700
#define CVSP_MONITORING_LOAD 701

// 버전 및 포트
#define CVSP_VER 0x01
#define CVSP_PORT 9000

// 페이로드 크기
#define CVSP_STANDARD_PAYLOAD_LENGTH 4096

// 커맨드
#define CVSP_JOINREQ 0x01
#define CVSP_JOINRES 0x02
#define CVSP_CHATTINGREQ 0x03
#define CVSP_CHATTINGRES 0x04
#define CVSP_OPERATIONREQ 0x05
#define CVSP_MONITORINGMSG 0x06
#define CVSP_LEAVEREQ 0x07
#define CVSP_SPAWN_OBJECT_REQ 0x08
#define CVSP_SPAWN_OBJECT_RES 0x09
#define CVSP_RPC_REQ 0x10
#define CVSP_RPC_NOPARAM_REQ 0x11
#define CVSP_RPC_RES 0x12
#define CVSP_RPC_NOPARAM_RES 0x13

// 옵션
#define CVSP_SUCCESS 0x01
#define CVSP_FAIL 0x02
#define CVSP_NEW_USER 0x03
#define CVSP_RPCTARGET_ALL 0x04
#define CVSP_RPCTARGET_SERVER 0x05


#ifdef WIN32
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;
#endif

using uint8 = u_char;
using uint16 = u_short;
using uint32 = u_int;
using ulong = u_long;

// 4바이트 단위로 맞춰주기
struct alignas(4) CVSPHeader
{
	uint8 cmd;
	uint8 option;
	uint16 packetLength;
};

// 64 바이트짜리, 오브젝트 스폰 정보.
struct ObjectSpawnInfo
{
	float posX;
	float posY;
	float posZ;
	float quatX;
	float quatY;
	float quatZ;
	float quatW;
	int ownerId;

	char objectName[32];
};

struct alignas(4) Legacy_TransformInfo
{
	float posX;
	float posY;
	float posZ;
	float quatX;
	float quatY;
	float quatZ;
	float quatW;
	int ownerId;
};

struct Legacy_RPCInfo
{
	char functionName[20];
	::byte rpcParams[96];
	::byte rpcParamTypes[8];
	int ownerId;
};

struct Legacy_RPCInfoNoParam
{
	char functionName[20];
	int ownerId;
};

struct Legacy_PlayerInfo
{
	char nickname[20];
	int id;
};

struct Legacy_RPCValueType
{
	static const ::byte UNDEFINED = (::byte)0x00;
	static const ::byte Int = (::byte)0x01;
	static const ::byte Float = (::byte)0x02;
	static const ::byte String = (::byte)0x03;
	static const ::byte Vec3 = (::byte)0x04;
	static const ::byte Quat = (::byte)0x05;
};

int SendCVSP(uint32 socketfd, uint8 cmd, uint8 option, void* payload, uint16 len);
int RecvCVSP(uint32 socketfd, uint8* cmd, uint8* option, void* payload, uint16 len);
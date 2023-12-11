#include "Legacy_CVSP.h"


int SendCVSP(uint32 socketfd, uint8 cmd, uint8 option, void* payload, uint16 len)
{
	CVSPHeader cvspHeader;
	uint32 packetSize;
	int result;

	// 패킷 크기 설정
	packetSize = len + sizeof(CVSPHeader);

	// 헤더 파일 설정
	cvspHeader.cmd = cmd;
	cvspHeader.option = option;
	cvspHeader.packetLength = packetSize;

	char* cvspPacket = (char*)malloc(packetSize);
	assert(cvspPacket != nullptr);

	ZeroMemory(cvspPacket, packetSize);
	// 먼저 헤더 크기만큼만 넣어본다.
	memcpy(cvspPacket, &cvspHeader, sizeof(CVSPHeader));

	// 페이로드가 존재한다면 페이로드만큼을 추가로 헤더 뒤쪽에 넣어준다.
	if (payload != nullptr)
	{
		memcpy(cvspPacket + sizeof(CVSPHeader), payload, len);
	}

	result = send(socketfd, cvspPacket, packetSize, 0);
	free(cvspPacket);

	return result;
}


int RecvCVSP(uint32 socketfd, uint8* cmd, uint8* option, void* payload, uint16 len)
{
	CVSPHeader cvspHeader;
	char extraPacket[CVSP_STANDARD_PAYLOAD_LENGTH];
	int recvSize;
	int payloadSize;
	int payloadCopySize;

	assert(payload != nullptr);
	ZeroMemory(extraPacket, sizeof(extraPacket));

	int curRead, lastRead;
	int ret = 0;

	// 잠깐 피킹만 하기
	recvSize = recv(socketfd, (char*)&cvspHeader, sizeof(cvspHeader), MSG_PEEK);

	if (recvSize < 0) return recvSize;

	curRead = 0;
	lastRead = cvspHeader.packetLength;

	// 데이터가 나눠서 보내지는 경우를 고려한 코드
	// extraPacket에 읽기
	while (curRead != cvspHeader.packetLength)
	{
		ret = recv(socketfd, extraPacket + curRead, lastRead, 0);
		if (ret < 0) return -1;

		curRead += ret;
		lastRead -= ret;
	}

	// 헤더 복사
	memcpy(&cvspHeader, extraPacket, sizeof(cvspHeader));
	payloadSize = cvspHeader.packetLength - sizeof(CVSPHeader);
	*cmd = cvspHeader.cmd;
	*option = cvspHeader.option;
	payloadCopySize = payloadSize;

	// 페이로드 복사
	if (payloadCopySize != 0)
	{
		// extraPacket에 헤더 이후 부분은 페이로드 부분임
		memcpy(payload, extraPacket + sizeof(cvspHeader), payloadCopySize);
	}

	return payloadCopySize;
}

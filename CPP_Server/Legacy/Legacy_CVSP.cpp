#include "Legacy_CVSP.h"


int SendCVSP(uint32 socketfd, uint8 cmd, uint8 option, void* payload, uint16 len)
{
	CVSPHeader cvspHeader;
	uint32 packetSize;
	int result;

	// ��Ŷ ũ�� ����
	packetSize = len + sizeof(CVSPHeader);

	// ��� ���� ����
	cvspHeader.cmd = cmd;
	cvspHeader.option = option;
	cvspHeader.packetLength = packetSize;

	char* cvspPacket = (char*)malloc(packetSize);
	assert(cvspPacket != nullptr);

	ZeroMemory(cvspPacket, packetSize);
	// ���� ��� ũ�⸸ŭ�� �־��.
	memcpy(cvspPacket, &cvspHeader, sizeof(CVSPHeader));

	// ���̷ε尡 �����Ѵٸ� ���̷ε常ŭ�� �߰��� ��� ���ʿ� �־��ش�.
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

	// ��� ��ŷ�� �ϱ�
	recvSize = recv(socketfd, (char*)&cvspHeader, sizeof(cvspHeader), MSG_PEEK);

	if (recvSize < 0) return recvSize;

	curRead = 0;
	lastRead = cvspHeader.packetLength;

	// �����Ͱ� ������ �������� ��츦 ����� �ڵ�
	// extraPacket�� �б�
	while (curRead != cvspHeader.packetLength)
	{
		ret = recv(socketfd, extraPacket + curRead, lastRead, 0);
		if (ret < 0) return -1;

		curRead += ret;
		lastRead -= ret;
	}

	// ��� ����
	memcpy(&cvspHeader, extraPacket, sizeof(cvspHeader));
	payloadSize = cvspHeader.packetLength - sizeof(CVSPHeader);
	*cmd = cvspHeader.cmd;
	*option = cvspHeader.option;
	payloadCopySize = payloadSize;

	// ���̷ε� ����
	if (payloadCopySize != 0)
	{
		// extraPacket�� ��� ���� �κ��� ���̷ε� �κ���
		memcpy(payload, extraPacket + sizeof(cvspHeader), payloadCopySize);
	}

	return payloadCopySize;
}

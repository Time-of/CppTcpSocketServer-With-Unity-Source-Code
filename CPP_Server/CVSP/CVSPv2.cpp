#include "pch.h"
#include "CVSPv2.h"

#include <cstdlib>
#include <fcntl.h>
#include <malloc.h>


int CVSPv2::Send(int socketFd, uint8 command, uint8 option)
{
	if (socketFd == SOCKET_ERROR)
	{
		LOG_ERROR(FMT("��ȿ���� ���� �����Դϴ�! fd: {}", socketFd));
		return -1;
	}


	CVSPv2Header header;
	constexpr int headerSize = sizeof(CVSPv2Header);
	header.command = command;
	header.option = option;
	header.packetLength = headerSize;


	char* cvspPacket = (char*)malloc(headerSize);
	assert(cvspPacket != nullptr);
	ZeroMemory(cvspPacket, headerSize);


	// ��� ����
	memcpy_s(cvspPacket, headerSize, &header, headerSize);
	int result = send(socketFd, cvspPacket, headerSize, 0);
	free(cvspPacket);
	return result;
}


int CVSPv2::Send(int socketFd, uint8 command, uint8 option, uint16 packetLength, void* payload)
{
	if (socketFd == SOCKET_ERROR)
	{
		LOG_ERROR(FMT("��ȿ���� ���� �����Դϴ�! fd: {}", socketFd));
		return -1;
	}


	CVSPv2Header header;
	constexpr int headerSize = sizeof(CVSPv2Header);
	uint16 packetSize = packetLength + headerSize;
	header.command = command;
	header.option = option;
	header.packetLength = packetSize;


	char* cvspPacket = (char*)malloc(packetSize);
	assert(cvspPacket != nullptr);
	ZeroMemory(cvspPacket, packetSize);


	// ��� ����
	memcpy_s(cvspPacket, packetSize, &header, headerSize);
	// ��Ŷ ����
	if (payload != nullptr)
	{
		memcpy_s(cvspPacket + headerSize, packetSize - headerSize, payload, packetLength);
	}


	int result = send(socketFd, cvspPacket, packetSize, 0);
	free(cvspPacket);
	return result;
}


int CVSPv2::Recv(int socketFd, uint8& outCommand, uint8& outOption, void* outPayload)
{
	if (socketFd == SOCKET_ERROR)
	{
		LOG_ERROR(FMT("��ȿ���� ���� �����Դϴ�! fd: {}", socketFd));
		return -1;
	}


	CVSPv2Header header;
	constexpr int headerSize = sizeof(CVSPv2Header);
	char receivedPacket[CVSPv2::STANDARD_PAYLOAD_LENGTH];
	ZeroMemory(receivedPacket, sizeof(receivedPacket));


	// ��� ���� ���� ���� + ����� ��Ŷ ���� ����
	int recvLength = recv(socketFd, (char*)&header, headerSize, MSG_PEEK);
	if (recvLength < 0 or recvLength != 4)
	{
		outOption = Option::Failed;
		LOG_CRITICAL(FMT("��� ũ�� ������! recvLength: {}", recvLength));
		return -1;
	}
	assert(header.packetLength <= CVSPv2::STANDARD_PAYLOAD_LENGTH);


	// ���� ��Ŷ Receive
	int nowRead = 0;
	int sizeNeedToRead = header.packetLength;
	while (nowRead < header.packetLength)
	{
		int result = recv(socketFd, receivedPacket + nowRead, sizeNeedToRead, 0);
		if (result < 0)
		{
			LOG_CRITICAL(FMT("��Ŷ�� ����� �������� ����!"));
			return -1;
		}

		nowRead += result;
		sizeNeedToRead -= result;
	}


	uint16 packetSize = header.packetLength - headerSize;
	outCommand = header.command;
	outOption = header.option;


	// ���� ��Ŷ ����
	if (packetSize != 0)
	{
		memcpy_s(outPayload, packetSize, receivedPacket + headerSize, packetSize);
	}
	return packetSize;
}


int CVSPv2::SendSerializedPacket(int socketFd, uint8 command, uint8 option, Serializer& serializer)
{
	auto [length, serializedPacket] = serializer.GetSerializedPacket();
	return SendSerializedPacket(socketFd, command, option, length, serializedPacket);
}


int CVSPv2::SendSerializedPacket(int socketFd, uint8 command, uint8 option, uint16 length, const SerializedPacket& serializedPacket)
{
	if (length == 0)
	{
		LOG_ERROR("SerializedPacket�� length�� 0�ε� ������ �մϴ�!!!");
		return -1;
	}
	else if (socketFd == SOCKET_ERROR)
	{
		LOG_ERROR(FMT("��ȿ���� ���� �����Դϴ�! fd: {}", socketFd));
		return -1;
	}


	// ��� ����
	CVSPv2Header header;
	constexpr int headerSize = sizeof(CVSPv2Header);
	header.command = command;
	header.option = option;
	header.packetLength = headerSize + length;


	// ��Ŷ�� ��� ����
	char* packet = (char*)malloc(header.packetLength);
	assert(packet != nullptr and "packet�� nullptr�Դϴ�!");
	ZeroMemory(packet, header.packetLength);
	memcpy_s(packet, header.packetLength, &header, headerSize);


	// ���� ����
	const uint16 serializedPacketLength = length - serializedPacket.typesCount - 1;
	packet[headerSize] = serializedPacket.typesCount; // typesCount ����
	// Data ����
	memcpy_s(packet + 5, header.packetLength - 5, serializedPacket.serializedData, serializedPacketLength);
	// DataTypes ����
	memcpy_s(packet + 5 + serializedPacketLength, header.packetLength - 5 - serializedPacketLength, serializedPacket.serializedDataTypes, serializedPacket.typesCount);


	int result = send(socketFd, packet, header.packetLength, 0);
	free(packet);
	return result;
}

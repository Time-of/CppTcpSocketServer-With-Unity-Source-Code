#include "pch.h"

#include "TcpServer.h"


TcpServer::TcpServer() noexcept
{
	bIsRunning = true;

	InitSocketLayer();
}


TcpServer::~TcpServer()
{
	bIsRunning = false;

	closesocket(serverSocket);
	CloseSocketLayer();
}


void TcpServer::Listen(int port)
{
	this->port = port;

	serverSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (serverSocket == INVALID_SOCKET)
	{
		LOG_ERROR(FMT("서버 소켓 생성 실패!", GetLastError()));
		CloseSocketLayer();

		return;
	}

	workerThreadPool = std::make_shared<ThreadPool>(16);
	assert(workerThreadPool != nullptr);

	lobby = std::make_shared<Lobby>();
	assert(lobby != nullptr);


	transitionVoid = std::make_shared<PlayerTransitionVOID>(lobby);
	assert(transitionVoid != nullptr);


	SOCKADDR_IN service;
	ZeroMemory(&service, sizeof(service));
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = htonl(INADDR_ANY);
	service.sin_port = htons(port);

	if (bind(serverSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR)
	{
		LOG_ERROR(FMT("bind 오류! GetLastError(): ", GetLastError()));
		closesocket(serverSocket);
		return;
	}

	if (listen(serverSocket, 5) == SOCKET_ERROR)
	{
		LOG_ERROR(FMT("listen 오류!", GetLastError()));
		closesocket(serverSocket);
		return;
	}

	LOG_INFO("연결 대기 중...");


	lobby->Receive();
}


bool TcpServer::InitSocketLayer()
{
	WORD version = MAKEWORD(2, 2);

	WSADATA wsaData;

	if (WSAStartup(version, &wsaData) != 0)
	{
		LOG_ERROR(FMT("WSAStartup() 오류!", GetLastError()));
		return false;
	}

	return true;
}


void TcpServer::CloseSocketLayer()
{
	if (WSACleanup() == SOCKET_ERROR)
	{
		LOG_ERROR(FMT("WSACleanup() 오류!", GetLastError()));
	}
}

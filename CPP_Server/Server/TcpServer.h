#pragma once

#include <memory>

#include "CVSP/CVSPv2.h"
#include "Game/Lobby.h"
#include "Game/PlayerTransitionVOID.h"
#include "Server/PlayerClient.h"
#include "Server/ThreadPool.h"


class TcpServer
{
private:
	explicit TcpServer() noexcept;


public:
	static TcpServer& GetInstance()
	{
		static TcpServer instance;
		return instance;
	}


	~TcpServer();


public:
	void Listen(int port = CVSPv2::PORT);


private:
	bool InitSocketLayer();
	void CloseSocketLayer();


public:
	bool GetIsRunning() const { return bIsRunning; }
	SOCKET GetSocket() const { return serverSocket; }

	std::shared_ptr<ThreadPool> GetWorkerThreadPool() const { return workerThreadPool; };
	std::shared_ptr<PlayerTransitionVOID> GetTransitionVoid() const { return transitionVoid; };


private:
	/** @brief 서버 가동 상태. */
	bool bIsRunning = false;
	int port = -1;
	SOCKET serverSocket = NULL;
	

private:
	std::shared_ptr<ThreadPool> workerThreadPool;
	std::shared_ptr<Lobby> lobby;
	std::shared_ptr<PlayerTransitionVOID> transitionVoid;


private:
	explicit TcpServer(const TcpServer& other) = delete;
	TcpServer& operator=(const TcpServer& other) = delete;
};

#define GetTcpServer TcpServer::GetInstance()
#define GetThreadPool TcpServer::GetInstance().GetWorkerThreadPool()
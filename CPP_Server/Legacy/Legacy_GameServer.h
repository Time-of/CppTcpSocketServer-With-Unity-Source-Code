#pragma once

#include <cassert>
#include <iostream>
#include <process.h>
#include <stack>
#include <string>
#include <vector>
#include <thread>

// 가장 최신 WinSock 버전
#include <Winsock2.h>
using namespace std;




struct ClientInfo
{
public:
	bool bIsConnected;
	string nickname;
	int id;
	SOCKET socket;
	HANDLE clientHandle;

	ClientInfo()
	{
		socket = NULL;
		bIsConnected = false;
		nickname = "";
		id = -1;
		clientHandle = NULL;
	}
};

typedef vector<ClientInfo>::iterator ClientInfoIter;


class Legacy_GameServer
{
public:
	Legacy_GameServer();
	~Legacy_GameServer();

	void Listen(int port);

	static UINT WINAPI ControlThread(LPVOID p);
	static UINT WINAPI ListenThread(LPVOID p);
	

private:
	bool InitializeSocketLayer();
	void CloseSocketLayer();


private:
	int portNum;
	SOCKET serverSocket;

	bool bIsRunning;
	SOCKET lastSocket;

	vector<ClientInfo> clientArray;
	stack<ClientInfoIter, vector<ClientInfoIter>> clientPools;
};

#include "Legacy_GameServer.h"
#include "Legacy_GameplayCalcs.h"
#include "Legacy_Serializer.h"

#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <mmsystem.h>
#include <mutex>

#include "Legacy_CVSP.h"
using namespace std;


Legacy_GameServer::Legacy_GameServer()
{
	portNum = 0;
	bIsRunning = true;

	InitializeSocketLayer();

	clientArray = vector<ClientInfo>(100, ClientInfo());

	for (auto iter = clientArray.begin(); iter != clientArray.end(); ++iter)
	{
		clientPools.push(iter);
	}
}


Legacy_GameServer::~Legacy_GameServer()
{
	bIsRunning = false;

	// 굳이 필요한가?
	//while (!clientPools.empty()) clientPools.pop();

	// 여기선 굳이 반복자를 사용하지 않아도 될 듯?
	for (auto& info : clientArray)
	{
		if (info.bIsConnected)
		{
			WaitForSingleObject(info.clientHandle, INFINITE);
			CloseHandle(info.clientHandle);
			closesocket(info.socket);
			info.bIsConnected = false;
		}
	}

	closesocket(serverSocket);
	CloseSocketLayer();
}


void Legacy_GameServer::Listen(int port)
{
	portNum = port;

	// 싱글턴 미리 초기화 수행
	Legacy_GameplayCalcs::GetInstance();

	thread listenThread(&Legacy_GameServer::ListenThread, this);
	listenThread.join();

	/*
	listenHandle = (HANDLE)_beginthreadex(nullptr, 0, Legacy_GameServer::ListenThread, this, 0, nullptr);

	if (listenHandle == nullptr)
	{
		cout << "리슨 스레드 오류!\n";
		return;
	}
	*/
}


UINT __stdcall Legacy_GameServer::ControlThread(LPVOID p)
{
	Legacy_GameServer* server = (Legacy_GameServer*)p;

	SOCKET connectSocket = server->lastSocket;
	assert(connectSocket != SOCKET_ERROR);
	vector<ClientInfo>& clientArray = server->clientArray;

	auto iter = find_if(clientArray.begin(), clientArray.end(), [&connectSocket](ClientInfo& info) -> bool
		{
			return info.socket == connectSocket;
		});

	assert(iter != clientArray.end());

	int id = iter - clientArray.begin() + 1;
	id = 100 - id;

	cout << "클라이언트 " << id << " 연결!\n";

	int recvLen = 0;


	// select 설정들
	fd_set fdReadSet, fdErrorSet, fdMaster;
	struct timeval tvs;

	FD_ZERO(&fdMaster);
	FD_SET(connectSocket, &fdMaster);
	tvs.tv_sec = 0;
	tvs.tv_usec = 100;


	// CVSP
	uint8 cmd;
	uint8 option;

	char extraPacket[CVSP_STANDARD_PAYLOAD_LENGTH - sizeof(CVSPHeader)];


	while (iter->bIsConnected)
	{
		// select를 이용한 비동기 통신
		fdReadSet = fdMaster;
		fdErrorSet = fdMaster;

		select((int)connectSocket + 1, &fdReadSet, nullptr, &fdErrorSet, &tvs);

		// read 성공 시 recv를 호출하도록 변경하여, Blocking 제거 효과
		if (FD_ISSET(connectSocket, &fdReadSet))
		{
			ZeroMemory(extraPacket, sizeof(extraPacket));

			recvLen = RecvCVSP((uint32)connectSocket, &cmd, &option, extraPacket, sizeof(extraPacket));

			if (recvLen == SOCKET_ERROR)
			{
				cout << "recv 오류: " << GetLastError() << "\n";
				break;
			}

			switch (cmd)
			{

			// 채팅 요청
			case CVSP_CHATTINGREQ:
			{
				//messageBuffer[recvLen] = '\0';

				char clientInfoText[CVSP_STANDARD_PAYLOAD_LENGTH - sizeof(CVSPHeader)] = "[";
				strcat_s(clientInfoText, sizeof(clientInfoText), to_string(id).c_str());
				strcat_s(clientInfoText, sizeof(clientInfoText), "]: ");
				strcat_s(clientInfoText, sizeof(clientInfoText), extraPacket);
				clientInfoText[strlen(clientInfoText)] = '\0';

				cmd = CVSP_CHATTINGRES;
				option = CVSP_SUCCESS;

				for (auto infoIter = clientArray.begin(); infoIter != clientArray.end(); ++infoIter)
				{
					if (!infoIter->bIsConnected) continue;

					int clientId = 100 - (infoIter - clientArray.begin() + 1);
					cout << clientId << "에게 메시지 전달\n";

					int sendResult = SendCVSP((uint32)infoIter->socket, cmd, option, clientInfoText, static_cast<uint16>(strlen(clientInfoText)));
					if (sendResult < 0) cout << "Send 오류!\n";
				}

				cout << "클라이언트" << clientInfoText << "\n";
				break;
			}


			// 이동 정보 요청 (Legacy_TransformInfo)
			case CVSP_OPERATIONREQ:
			{
				// 본인 제외 다른 클라이언트 모두에게 뿌리기
				for (auto infoIter = clientArray.begin(); infoIter != clientArray.end(); ++infoIter)
				{
					if (!infoIter->bIsConnected) continue;
					int clientId = 100 - (infoIter - clientArray.begin() + 1);
					if (clientId == id) continue;

					int sendResult = SendCVSP((uint32)infoIter->socket, CVSP_MONITORINGMSG, CVSP_SUCCESS, extraPacket, static_cast<uint16>(sizeof(Legacy_TransformInfo)));
					//cout << "TransformInfo를 " << id << " 에게서 " << clientId << " 에게 전송 " << ((sendResult >= 0) ? "성공!\n" : "실패!\n");
				}
				break;
			}


			// RPC 요청 (파라미터 보유)
			case CVSP_RPC_REQ:
			{
				switch (option)
				{

				// 모든 클라이언트에게 뿌릴 RPC
				case CVSP_RPCTARGET_ALL:
				{
					Legacy_RPCInfo info;
					ZeroMemory(&info, sizeof(Legacy_RPCInfo));
					memcpy(&info, extraPacket, sizeof(Legacy_RPCInfo));

					for (auto infoIter = clientArray.begin(); infoIter != clientArray.end(); ++infoIter)
					{
						if (!infoIter->bIsConnected) continue;

						int sendResult = SendCVSP((uint32)infoIter->socket, CVSP_RPC_RES, CVSP_SUCCESS, extraPacket, static_cast<uint16>(sizeof(Legacy_RPCInfo)));
						if (sendResult < 0) cout << "클라이언트 [" << infoIter->id << "] 에게 RPC 전송 실패!\n";
					}
					cout << "RPC 요청 [" << info.functionName << "] 모두에게 전송 완료!\n";
					break;
				}

				// 서버에서 실행할 RPC
				case CVSP_RPCTARGET_SERVER:
				{
					Legacy_RPCInfo info;
					ZeroMemory(&info, sizeof(Legacy_RPCInfo));
					memcpy(&info, extraPacket, sizeof(Legacy_RPCInfo));

					cout << "서버로 보내진 RPC 요청 [" << info.functionName << "] 수신 완료!\n";


					// 역직렬화
					std::vector<std::shared_ptr<std::any>> args { Legacy_Serializer::Deserialize(info.rpcParams, info.rpcParamTypes) };
					

					// 계산
					CalcResult calcResult = Legacy_GameplayCalcs::GetInstance().InvokeFunction(info.functionName, args);
					if (!calcResult.bSuccessed) break;
					
					
					Legacy_RPCInfo resultInfo;
					ZeroMemory(&resultInfo, sizeof(Legacy_RPCInfo));
					resultInfo.ownerId = info.ownerId;
					strcpy_s(resultInfo.functionName, calcResult.broadcastFunctionName);
					
					// 직렬화
					vector<::byte> outSerializedBytes;
					bool serializeResult = Legacy_Serializer::Serialize(calcResult.result, calcResult.typeInfos, outSerializedBytes);
					if (!serializeResult) cout << "직렬화 실패!\n";
					std::copy(outSerializedBytes.begin(), outSerializedBytes.end(), resultInfo.rpcParams);
					std::copy(calcResult.typeInfos.begin(), calcResult.typeInfos.end(), resultInfo.rpcParamTypes);


					// 결과 전송
					for (auto infoIter = clientArray.begin(); infoIter != clientArray.end(); ++infoIter)
					{
						if (!infoIter->bIsConnected) continue;

						// 결과 Legacy_RPCInfo 전파
						int sendResult = SendCVSP((uint32)infoIter->socket, CVSP_RPC_RES, CVSP_SUCCESS, &resultInfo, static_cast<uint16>(sizeof(Legacy_RPCInfo)));
						if (sendResult < 0) cout << "클라이언트 [" << infoIter->id << "] 에게 RPC 전송 실패!\n";
					}
					cout << "서버에서 계산한 후 RPC [" << resultInfo.functionName << "] 모두에게 전송 완료!\n";

					break;
				}

				default:
				{
					cout << "RPC 요청의 option이 유효하지 않음!\n";
					break;
				}

				}
				break;
			}


			// RPC 요청 (파라미터 미보유)
			case CVSP_RPC_NOPARAM_REQ:
			{
				switch (option)
				{
				case CVSP_RPCTARGET_ALL:
				{
					Legacy_RPCInfo info;
					ZeroMemory(&info, sizeof(Legacy_RPCInfo));
					memcpy(&info, extraPacket, sizeof(Legacy_RPCInfo));

					for (auto infoIter = clientArray.begin(); infoIter != clientArray.end(); ++infoIter)
					{
						if (!infoIter->bIsConnected) continue;

						int sendResult = SendCVSP((uint32)infoIter->socket, CVSP_RPC_NOPARAM_RES, CVSP_SUCCESS, extraPacket, static_cast<uint16>(sizeof(Legacy_RPCInfoNoParam)));
						if (sendResult < 0) cout << "클라이언트 [" << infoIter->id << "] 에게 NOPARAM RPC 전송 실패!\n";
					}
					cout << "NOPARAM RPC 요청 [" << info.functionName << "] 모두에게 전송 완료!\n";
					break;
				}

				default:
				{
					cout << "NOPARAM RPC 요청의 option이 유효하지 않음!\n";
					break;
				}
				}

				break;
			}


			// JoinServer 요청
			case CVSP_JOINREQ:
			{
				// 클라이언트에서는 닉네임만 보냄, id는 서버가 할당
				char nickname[20];
				strcpy_s(nickname, 19, extraPacket);

				iter->id = id;
				iter->nickname = nickname;

				cout << "클라이언트 " << id << "에서 Join 요청! 닉네임: " << iter->nickname << "\n";
				cout << "닉네임 길이: " << iter->nickname.length() << "\n";

				Legacy_PlayerInfo myInfo;
				myInfo.id = id;
				strcpy_s(myInfo.nickname, 19, nickname);

				// 요청자 본인에게 id 전송
				if (SendCVSP((uint32)iter->socket, CVSP_JOINRES, CVSP_SUCCESS, (int*)&id, static_cast<uint16>(sizeof(int))) < 0)
				{
					cout << "클라이언트 " << id << "에서 Join 요청 응답하기 위해 Send하는 데 오류 발생!\n";
				}

				
				for (auto infoIter = clientArray.begin(); infoIter != clientArray.end(); ++infoIter)
				{
					if (!infoIter->bIsConnected or infoIter->id == id) continue;
					//int clientId = 100 - (infoIter - clientArray.begin() + 1);

					Legacy_PlayerInfo info;
					info.id = infoIter->id;
					strcpy_s(info.nickname, 19, infoIter->nickname.c_str());

					// 방금 접속한 사람에게, 기존 접속자의 정보(Legacy_PlayerInfo)를 알려주기
					int sendResult = SendCVSP((uint32)iter->socket, CVSP_JOINRES, CVSP_NEW_USER, &info, static_cast<uint16>(sizeof(Legacy_PlayerInfo)));
					cout << "클라이언트 [" << iter->id << "] " << iter->nickname << "에게 [" << infoIter->id << "] " << infoIter->nickname << "의 Join 정보 전송 " << ((sendResult >= 0) ? "성공!\n" : "실패...\n");

					// 방급 접속한 유저 정보를 기존 유저들에게 전송하기
					sendResult = SendCVSP((uint32)infoIter->socket, CVSP_JOINRES, CVSP_NEW_USER, &myInfo, static_cast<uint16>(sizeof(Legacy_PlayerInfo)));
					cout << "클라이언트 [" << infoIter->id << "] 에게 [" << iter->id << "] 가 새로 Join했음을 알리는 데 " << ((sendResult >= 0) ? "성공!\n" : "실패...\n");
				}
				break;
			}


			// 오브젝트 스폰 요청
			case CVSP_SPAWN_OBJECT_REQ:
			{
				cout << "클라이언트 " << id << "에서 Object Spawn 요청!\n";

				// **요청자를 제외하고** 클라이언트에 응답 뿌리기
				// 요청자는 본인이 즉석해서 로컬에 생성하는 방식. (클라이언트 쪽)
				for (auto infoIter = clientArray.begin(); infoIter != clientArray.end(); ++infoIter)
				{
					if (!infoIter->bIsConnected) continue;
					int clientId = 100 - (infoIter - clientArray.begin() + 1);
					if (clientId == id) continue;

					int sendResult = SendCVSP((uint32)infoIter->socket, CVSP_SPAWN_OBJECT_RES, CVSP_SUCCESS, extraPacket, static_cast<uint16>(sizeof(ObjectSpawnInfo)));
					cout << "클라이언트 " << clientId << "에게 Spawn 요청 응답" << ((sendResult >= 0) ? " 성공!\n" : "하기 위해 Send하는 데 오류 발생!\n");
				}
				break;
			}


			// 종료 요청
			case CVSP_LEAVEREQ:
			{
				cout << "소켓 연결을 종료합니다!\n";
				iter->bIsConnected = false;
				break;
			}
			}
		}
	}

	iter->bIsConnected = false;
	closesocket(connectSocket);
	server->clientPools.push(iter);
	cout << "클라이언트 " << id << " 연결 종료됨!!: " << GetLastError() << "\n";

	return 0;
}


UINT __stdcall Legacy_GameServer::ListenThread(LPVOID p)
{
	Legacy_GameServer* server = (Legacy_GameServer*)p;
	SOCKET& serverSocket = server->serverSocket;
	SOCKADDR_IN service;

	// 소켓 생성
	serverSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (serverSocket == INVALID_SOCKET)
	{
		cout << "서버 소켓 생성 실패\n";
		WSACleanup();

		return -1;
	}


	{
		ZeroMemory(&service, sizeof(service));
		service.sin_family = AF_INET;
		service.sin_addr.s_addr = htonl(INADDR_ANY);
		service.sin_port = htons(server->portNum);
	}


	if (::bind(serverSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR)
	{
		cout << "바인드 오류\n";
		closesocket(serverSocket);
		return -1;
	}


	// 백로그가 5
	if (listen(serverSocket, 5) == SOCKET_ERROR)
	{
		cout << "리슨 오류\n";
		closesocket(serverSocket);
		return -1;
	}


	cout << "서버 시작: 연결 대기 중\n";


	while (server->bIsRunning)
	{
		SOCKET connectSocket;
		connectSocket = accept(serverSocket, nullptr, nullptr);

		if (connectSocket > 0)
		{
			// 커넥션 풀링을 체크
			if (server->clientPools.empty())
			{
				closesocket(connectSocket);

				continue;
			}

			auto iter = server->clientPools.top();
			server->clientPools.pop();

			iter->socket = connectSocket;
			server->lastSocket = connectSocket;

			iter->bIsConnected = true;
			iter->clientHandle = (HANDLE)_beginthreadex(nullptr, 0, Legacy_GameServer::ControlThread, server, 0, nullptr);
			//iter->clientThread = thread([&iter, &server]() -> UINT { return Legacy_GameServer::ControlThread(server); } );
			//iter->clientThread.join();
		}

		Sleep(50); // 퍼포먼스를 위한 부분
	}


	return 0;
}


bool Legacy_GameServer::InitializeSocketLayer()
{
	WORD version = MAKEWORD(2, 2);
	
	WSADATA wsaData;

	// WinSock 동적 연결 라이브러리(DLL)를 초기화하고 WinSock 구현이 앱 요구사항을 충족시키는지 확인
	if (WSAStartup(version, &wsaData) != 0)
	{
		cout << "WSAStartup에서 오류 발생!\n";
		return false;
	}

	return true;
}


void Legacy_GameServer::CloseSocketLayer()
{
	// 윈속 사용 통신 끝내기
	if (WSACleanup() == SOCKET_ERROR)
	{
		cout << "WSACleanup 실패, 원인: " << WSAGetLastError();
	}
}

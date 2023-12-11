using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;
using System.Net.Sockets;
using System.Net;
using System.Text;
using UnityEngine;
using CVSP;
using System;
using UnityEngine.Events;
using UnityEngine.AdaptivePerformance;
using UnityEngine.SceneManagement;
using static UnityEngine.AdaptivePerformance.Provider.AdaptivePerformanceSubsystemDescriptor;
using System.Linq;


/// <summary>
/// <author>이성수(Time-of)</author>
/// 서버와 연결을 담당하는 클래스입니다. <br/>
/// 더불어, 액터들의 관련성에 따라 업데이트해야 할 ActorNetObserver들을 참조하고 있습니다. <br/>
/// </summary>
public class PlayerClient : MonoBehaviour
{
	public static PlayerClient instance;

	private Socket socket;
	public Socket GetSocket() { return socket; }
	public bool bIsConnected { get; private set; }


	public Queue<UnityAction> OnLoginSuccessedCallbackQueue = new();
	public Queue<UnityAction> OnLoginFailedCallbackQueue = new();
	public UnityAction<bool, JoinRoomResponseInfo> OnJoinRoomResponseDelegate;
	public Queue<UnityAction> OnDisconnectedCallbackQueue = new();
	public UnityAction<bool, RoomInfoList> OnRefreshRoomsListReceivedDelegate;
	public UnityAction<RoomUserRefreshedInfo> OnRoomUserRefreshedDelegate;
	public UnityAction<int> OnRoomMasterUpdatedDelegate;
	public Queue<UnityAction> OnLeaveRoomSuccessedCallbackQueue = new();


	private Thread recvThread = null;


	public PlayerInfo playerInfo { get; private set; }
	public bool bIsInGame { get; private set; }

	public static Dictionary<uint, ActorNetObserver> actorObservers = new();
	public A_Controller myController = null;


	public PlayerClient()
	{
		socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.IP);
		playerInfo = null;
		bIsInGame = false;
	}


	#region 유니티 함수들
	public void Awake()
	{
		Application.targetFrameRate = 60; // 테스트 용도. 백그라운드에서 자꾸 프레임이 낮아짐
		Application.runInBackground = true;

		if (instance == null)
		{
			instance = this;
		}
		else if (instance != this)
		{
			Destroy(gameObject);
		}

		SceneManager.sceneLoaded += OnLevelLoaded;
		DontDestroyOnLoad(gameObject);
	}


	private void OnLevelLoaded(Scene sceneRef, LoadSceneMode loadSceneMode)
	{
		if (actorObservers != null)
		{
			foreach (var pair in actorObservers)
			{
				if (pair.Value != null && pair.Value.gameObject != null)
				{
					Destroy(pair.Value.gameObject);
				}
			}

			actorObservers.Clear();
		}


		FLGameplayHelper.actorDictionary.Clear();
		myController = null;


		// 관련 큐 전부 비우기
		OnLoginSuccessedCallbackQueue.Clear();
		OnLoginFailedCallbackQueue.Clear();
		OnJoinRoomResponseDelegate = null;
		OnDisconnectedCallbackQueue.Clear();
		OnRefreshRoomsListReceivedDelegate = null;
		OnRoomUserRefreshedDelegate = null;
		OnRoomMasterUpdatedDelegate = null;
		OnLeaveRoomSuccessedCallbackQueue.Clear();


		if (!bIsInGame)
		{
			return;
		}


		actorObservers.EnsureCapacity(16);


		// 게임 씬 로드 완료 메시지 전송
		Debug.Log("PlayerClient: 게임 씬 로드 완료 메시지 전송!");
		LoadSceneCompleteInfo info = new(0, 0, playerInfo.playerId, sceneRef.name);
		CVSPv2Serializer serializer = new();
		serializer.Push(info);
		serializer.QuickSendSerializedAndInitialize(CVSPv2.Command.PlayerHasLoadedGameScene, CVSPv2.Option.Success, true);
	}


	public void OnDestroy()
	{
		if (bIsConnected)
		{
			Debug.Log("<b>OnDestroy()에서 DisconnectFromServer() 호출!</b>");
			DisconnectFromServer();
		}
	}
	#endregion


	#region 서버 연결
	public bool ConnectToServer(string ipAddressString)
	{
		return ConnectToServer(ipAddressString, CVSPv2.PORT);
	}


	public bool ConnectToServer(string ipAddressString, int port)
	{
		if (socket.Connected)
		{
			return true;
		}

		try
		{
			IPAddress parsedIP = IPAddress.Parse(ipAddressString);
			socket.Connect(new IPEndPoint(parsedIP, port));

            // EUC-KR 인코딩을 위해 싱글톤 인스턴스 등록
            Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
		}
		catch (Exception e)
		{
			Debug.Log("<b>PlayerClient: 서버 연결 실패!</b> : " + e.Message);
			return false;
		}

		if (socket.Connected)
		{
			recvThread = new Thread(ReceiveThread);
			recvThread.Start();
			
			return true;
		}
		else return false;
	}


	public void DisconnectFromServer()
	{
		Debug.Log("PlayerClient: Disconnect 요청 송신!");
		
		OnDisconnected();
	}


	private void OnDisconnected()
	{
		SyncActorWorker.bNeedToWork = false;

		if (bIsConnected)
		{
			while (OnDisconnectedCallbackQueue.TryDequeue(out UnityAction dequeued))
			{
				ThreadWorksDistributor.instance.actionsQueue.Enqueue(dequeued);
			}

			CVSPv2.Send(socket, CVSPv2.Command.Disconnect, CVSPv2.Option.Success);
		}

		lock (socket)
		{
			socket.Close();
		}

		bIsConnected = false;
	}
	#endregion


	public int Send(byte command, byte option = CVSPv2.Option.Success)
	{
		return CVSPv2.Send(socket, command, option);
	}


	public void ReceiveThread()
	{
		bIsConnected = true;

		byte outCommand = 0, outOption = 0;
		const int maxPacketSize = CVSPv2.STANDARD_PAYLOAD_LENGTH - 4;
		byte[] receivedPacket = new byte[maxPacketSize]; // 헤더 크기 하드코딩
		Span<byte> byteSpan = new Span<byte>(receivedPacket);


		try
		{
			Debug.Log("PlayerClient: ReceiveThread 시작!");

			while (bIsConnected)
			{
				byteSpan.Fill(0);

				int recvLength = CVSPv2.Recv(socket, ref outCommand, ref outOption, ref receivedPacket);

				if (recvLength < 0)
				{
					Debug.LogWarning("ServerConnector: 경고! Recv 오류: " + recvLength);
					continue;
				}

				//Debug.Log("<b>Receive 받은 Command: " + outCommand + ", Option: " + outOption + "</b>");
				//Debug.Log("PlayerClient: recvLength: " + recvLength);


				switch (outCommand)
				{
					// 액터 트랜스폼 동기화
					case (CVSPv2.Command.SyncActorTransform | CVSPv2.Command.RESPONSE):
					{
						CVSPv2Serializer serializer = new();
							/*
							SyncActorWorker.dataQueue.Enqueue(new()
									{
										option = outOption,
										actorSyncSerializer = new(recvLength, receivedPacket, true)
									});
									*/

						serializer.InterpretSerializedPacket(recvLength, receivedPacket, true);

						switch (outOption)
						{

							case CVSPv2.Option.Success:
								{
									TransformInfo transformInfo = new();
									transformInfo.Deserialize(serializer);
									if (PlayerClient.actorObservers.TryGetValue(transformInfo.actorGuid, out ActorNetObserver observer))
									{
										observer.netPosition = transformInfo.position.Get();
										observer.netRotation = transformInfo.rotation.Get();
										//Debug.Log("워커 스레드 트랜스폼: " + transformInfo.actorGuid + ", pos: " + observer.netPosition + ", rot: " + observer.netRotation);
									}
								}
								break;

							case CVSPv2.Option.SyncActorPositionOnly:
								{
									ActorPositionInfo positionInfo = new();
									positionInfo.Deserialize(serializer);
									if (PlayerClient.actorObservers.TryGetValue(positionInfo.actorGuid, out ActorNetObserver observer))
									{
										observer.netPosition = positionInfo.position.Get();
										//Debug.Log("워커 스레드 포지션: " + positionInfo.actorGuid + ", pos: " + observer.netPosition);
									}
								}
								break;

							case CVSPv2.Option.SyncActorRotationOnly:
								{
									ActorRotationInfo rotationInfo = new();
									rotationInfo.Deserialize(serializer);
									if (PlayerClient.actorObservers.TryGetValue(rotationInfo.actorGuid, out ActorNetObserver observer))
									{
										observer.netRotation = rotationInfo.rotation.Get();
										//Debug.Log("워커 스레드 로테이션: " + rotationInfo.actorGuid + ", rot: " + observer.netRotation);
									}
								}
								break;

							default:
								Debug.LogWarning("SyncActorWorker: 이상한 option이 들어옴!: " + outOption);
								break;

						}




							break;
					}


					// 채팅 응답
					case (CVSPv2.Command.Chatting | CVSPv2.Command.RESPONSE):
					{
						string message = Serializer.DeserializeEucKrString(receivedPacket, recvLength);
						Debug.Log("ServerConnector: 서버로부터 채팅 응답! : " + message);

						ChattingInputBox.chattingQueue.Enqueue(message);

						break;
					}


					// 액터 스폰 응답
					case (CVSPv2.Command.SpawnActor | CVSPv2.Command.RESPONSE):
					{
						Debug.Log("SpawnActor 응답 수신!");

						CVSPv2Serializer serializer = new();
						serializer.InterpretSerializedPacket(recvLength, receivedPacket, true);
						ActorSpawnParams spawnParams = new();
						spawnParams.Deserialize(serializer);

						ThreadWorksDistributor.instance.actionsQueue.Enqueue(() => FLGameplayHelper.SpawnActor(spawnParams));

						break;
					}


					// 액터에 옵저버 부착 여부 통보 받음
					case (CVSPv2.Command.ActorNeedToObserved | CVSPv2.Command.RESPONSE):
					{
						CVSPv2Serializer serializer = new();
						serializer.InterpretSerializedPacket(recvLength, receivedPacket, true);

						if (outOption == CVSPv2.Option.Success)	
						{
							Debug.Log("ActorNeedToObserved Success 응답 수신! ActorNetObserver을 로컬에 생성!");
							TransformInfo transformInfo = new();
							transformInfo.Deserialize(serializer);

							ThreadWorksDistributor.instance.actionsQueue.Enqueue(() => FLGameplayHelper.SpawnActorNetObserver(transformInfo));
						}
						else
						{
							Debug.Log("ActorNeedToObserved Failed 응답 수신! ActorNetObserver 제거!");
							serializer.Get(out uint observedActorGuid);
							if (actorObservers.TryGetValue(observedActorGuid, out ActorNetObserver actorObserver))
							{
								ThreadWorksDistributor.instance.actionsQueue.Enqueue(() => Destroy(actorObserver.gameObject));
							}
						}

						break;
					}


					// 매우 간략한 버전의 RPC
					case (CVSPv2.Command.Rpc | CVSPv2.Command.RESPONSE):
					{
						/*
						CVSPv2Serializer serializer = new();
						serializer.serializedDataArray = receivedPacket.Take(recvLength).ToArray();
						serializer.Get(out string funcName).Get(out uint actorGuid);
						*/

						CVSPv2Serializer serializer = new();
						serializer.InterpretSerializedPacket(recvLength, receivedPacket, true);
						RpcInfo rpcInfo = new();
						rpcInfo.Deserialize(serializer);

						Debug.Log("RPC 응답 받음: 액터 [" + "]에 대한 함수 " + rpcInfo.functionName);

						if (FLGameplayHelper.actorDictionary.TryGetValue(rpcInfo.actorGuid, out Actor foundActor))
						{
							if (foundActor != null)
							{
								object[] args = null;

								if (rpcInfo.functionName == "OnDamaged")
								{
									args = new object[1];
									serializer.Get(out float damage);
									args[0] = damage;
								}
								else if (rpcInfo.functionName == "PerformAttack")
								{
									args = new object[1];
									serializer.Get(out int randomAttack);
									args[0] = randomAttack;
								}

								ThreadWorksDistributor.instance.actionsQueue.Enqueue(() => foundActor.GetType().GetMethod(rpcInfo.functionName).Invoke(foundActor, args));
							}
						}
						break;
					}


					// 서버 로그인
					case (CVSPv2.Command.LoginToServer | CVSPv2.Command.RESPONSE):
					{
						if (outOption == CVSPv2.Option.Success)
						{
							CVSPv2Serializer serializer = new();
							serializer.InterpretSerializedPacket(recvLength, receivedPacket, true);

							playerInfo = new PlayerInfo();
							playerInfo.Deserialize(serializer);


							while (OnLoginSuccessedCallbackQueue.TryDequeue(out UnityAction dequeued))
							{
								ThreadWorksDistributor.instance.actionsQueue.Enqueue(dequeued);
							}

							Debug.Log("<color=green>로그인 성공! 내 ID: " + playerInfo.playerId + ", 닉네임: " + playerInfo.nickname + "</color>");

							break;
						}

						while (OnLoginFailedCallbackQueue.TryDequeue(out UnityAction dequeued))
						{
							ThreadWorksDistributor.instance.actionsQueue.Enqueue(dequeued);
						}
						Debug.LogWarning("<color=red>로그인 실패!! 닉네임 혹은 비밀번호가 잘못되었습니다.</color>");
						

						break;
					}


					// 방 입장 응답
					case (CVSPv2.Command.JoinRoom | CVSPv2.Command.RESPONSE):
					{
						if (outOption != CVSPv2.Option.Success)
						{
							Debug.LogWarning("<b>방 입장 실패!</b>");

							if (OnJoinRoomResponseDelegate != null)
								ThreadWorksDistributor.instance.actionsQueue.Enqueue(() => OnJoinRoomResponseDelegate(false, null));
							break;
						}

						Debug.Log("<b>방 입장 성공!</b>");
						CVSPv2Serializer serializer = new();
						serializer.InterpretSerializedPacket(recvLength, receivedPacket, true);


						JoinRoomResponseInfo responseInfo = new();
						responseInfo.Deserialize(serializer);


						Debug.Log("방[" + responseInfo.roomGuid + "] 입장 정보 출력 중...");
						Debug.Log("방 이름: " + responseInfo.roomName + ", 방장 ID: " + responseInfo.roomMasterPlayerId);
						for (int i = 0; i < 4; ++i)
							Debug.Log("플레이어[" + responseInfo.players[i].playerId + "]의 닉네임: " + responseInfo.players[i].nickname);

						if (OnJoinRoomResponseDelegate != null)
							ThreadWorksDistributor.instance.actionsQueue.Enqueue(() => OnJoinRoomResponseDelegate(true, responseInfo));

						break;
					}


					// 방 정보 불러오기
					case (CVSPv2.Command.LoadRoomsInfo | CVSPv2.Command.RESPONSE):
					{
						if (outOption != CVSPv2.Option.Success)
						{
							Debug.LogWarning("LoadRoomsInfo 실패 응답 수신!");
							if (OnRefreshRoomsListReceivedDelegate != null)
							{
								ThreadWorksDistributor.instance.actionsQueue.Enqueue(() => OnRefreshRoomsListReceivedDelegate(false, null));
							}
							break;
						}

						Debug.Log("LoadRoomsInfo 응답 수신!");

						CVSPv2Serializer serializer = new();
						serializer.InterpretSerializedPacket(recvLength, receivedPacket, true);


						RoomInfoList roomInfoList = new();
						roomInfoList.Deserialize(serializer);
						if (OnRefreshRoomsListReceivedDelegate != null)
						{
							ThreadWorksDistributor.instance.actionsQueue.Enqueue(() => OnRefreshRoomsListReceivedDelegate(true, roomInfoList));
						}

						break;
					}


					// 방의 유저 정보 갱신됨
					case (CVSPv2.Command.RoomUserRefreshed | CVSPv2.Command.RESPONSE):
					{
						Debug.Log("RoomUserRefreshed 응답 수신!");

						if (outOption == CVSPv2.Option.Success)
						{
							CVSPv2Serializer serializer = new();
							serializer.InterpretSerializedPacket(recvLength, receivedPacket, true);


							RoomUserRefreshedInfo refreshedInfo = new();
							refreshedInfo.Deserialize(serializer);

							Debug.Log("RoomUserRefreshed 결과: " + refreshedInfo.refreshedUserIndex + "번 인덱스의 [" + refreshedInfo.playerInfo.playerId + "], 닉네임: " + refreshedInfo.playerInfo.nickname);

							if (OnRoomUserRefreshedDelegate != null)
							{
								ThreadWorksDistributor.instance.actionsQueue.Enqueue(() => OnRoomUserRefreshedDelegate(refreshedInfo));
							}
						}
						else if (outOption == CVSPv2.Option.NewRoomMaster)
						{
							Debug.Log("방장 변경 정보 수신!");

							CVSPv2Serializer serializer = new();
							serializer.InterpretSerializedPacket(recvLength, receivedPacket, true);


							PlayerInfo info = new();
							info.Deserialize(serializer);


							if (OnRoomMasterUpdatedDelegate != null)
							{
								ThreadWorksDistributor.instance.actionsQueue.Enqueue(() => OnRoomMasterUpdatedDelegate(info.playerId));
							}
						}
						else
						{
							Debug.LogWarning("RoomUserRefreshed에서 알 수 없는 옵션을 수신!");
						}
						

						break;
					}


					// 방 떠나기
					case (CVSPv2.Command.LeaveRoom | CVSPv2.Command.RESPONSE):
					{
						if (outOption == CVSPv2.Option.Success)
						{
							Debug.Log("LeaveRoom Success 수신!");

							while (OnLeaveRoomSuccessedCallbackQueue.TryDequeue(out UnityAction dequeued))
							{
								ThreadWorksDistributor.instance.actionsQueue.Enqueue(dequeued);
							}
						}

						break;
					}


					// 게임 시작
					case (CVSPv2.Command.StartGame | CVSPv2.Command.RESPONSE):
					{
						Debug.Log("StartGame 수신!");

						if (outOption == CVSPv2.Option.Success)
						{
							bIsInGame = true;
							ThreadWorksDistributor.instance.actionsQueue.Enqueue(() => SceneManager.LoadScene("GameScene"));
						}

						break;
					}


					// 유저 한 명이 로딩 끝남! (서로 간 로딩 상태 동기화)
					case (CVSPv2.Command.PlayerHasLoadedGameScene | CVSPv2.Command.RESPONSE):
					{
						Debug.Log("PlayerHasLoadedGameScene 수신!");

						
						CVSPv2Serializer serializer = new();
						serializer.InterpretSerializedPacket(recvLength, receivedPacket, true);
						LoadSceneCompleteInfo info = new();
						info.Deserialize(serializer);


						if (outOption == CVSPv2.Option.Success)
						{
							Debug.Log("<color=green><b>[" + info.playerId + "]가 씬 로딩 성공!</b></color>");
							Debug.Log("<color=green><b>현재 로딩 상황: [ " 
								+ info.currentLoadedPlayerCount + " / " + info.validPlayerCount + " ]</b></color>");
						}
						else
						{
							Debug.Log("<color=orange><b>[" + info.playerId + "]가 씬 로드 실패!</b></color>");
							Debug.Log("<color=green><b>현재 로딩 상황: [ " 
								+ info.currentLoadedPlayerCount + " / " + info.validPlayerCount + " ]</b></color>");
						}


						break;
					}


					// 모든 유저가 로딩이 끝남! 을 서버로부터 전송받음!
					case (CVSPv2.Command.AllPlayersInRoomHaveFinishedLoading | CVSPv2.Command.RESPONSE):
					{
						Debug.Log("AllPlayersInRoomHaveFinishedLoading 수신!");

						if (outOption != CVSPv2.Option.Success)
						{
							Debug.LogWarning("<b><color=red>AllPlayersInRoomHaveFinishedLoading 실패 응답 받음!!</b></color>");
							break;
						}

						Debug.Log("PlayerController 액터 스폰 요청!");
						ActorSpawnParams controllerParams = new()
						{
							playerId = playerInfo.playerId,
							transform = new(),
							actorName = "PlayerController"
						};
						CVSPv2Serializer serializer = new();
						serializer.Push(controllerParams);
						serializer.QuickSendSerializedAndInitialize(CVSPv2.Command.SpawnActor, CVSPv2.Option.Success, true);

						break;
					}
				}
			}
		}
		catch (Exception e)
		{
			Debug.LogWarning("ServerConnector: 오류로 인해 서버와 연결 종료! 메시지: " + e.Message);
			Debug.LogWarning("호출 스택: " + e.StackTrace);
			Debug.LogWarning("Data: " + e.Data);

			OnDisconnected();

			return;
		}

		Debug.Log("PlayerClient: ReceiveThread 종료!");
	}
}

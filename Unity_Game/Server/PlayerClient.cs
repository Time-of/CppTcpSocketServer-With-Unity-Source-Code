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
/// <author>�̼���(Time-of)</author>
/// ������ ������ ����ϴ� Ŭ�����Դϴ�. <br/>
/// ���Ҿ�, ���͵��� ���ü��� ���� ������Ʈ�ؾ� �� ActorNetObserver���� �����ϰ� �ֽ��ϴ�. <br/>
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


	#region ����Ƽ �Լ���
	public void Awake()
	{
		Application.targetFrameRate = 60; // �׽�Ʈ �뵵. ��׶��忡�� �ڲ� �������� ������
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


		// ���� ť ���� ����
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


		// ���� �� �ε� �Ϸ� �޽��� ����
		Debug.Log("PlayerClient: ���� �� �ε� �Ϸ� �޽��� ����!");
		LoadSceneCompleteInfo info = new(0, 0, playerInfo.playerId, sceneRef.name);
		CVSPv2Serializer serializer = new();
		serializer.Push(info);
		serializer.QuickSendSerializedAndInitialize(CVSPv2.Command.PlayerHasLoadedGameScene, CVSPv2.Option.Success, true);
	}


	public void OnDestroy()
	{
		if (bIsConnected)
		{
			Debug.Log("<b>OnDestroy()���� DisconnectFromServer() ȣ��!</b>");
			DisconnectFromServer();
		}
	}
	#endregion


	#region ���� ����
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

            // EUC-KR ���ڵ��� ���� �̱��� �ν��Ͻ� ���
            Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
		}
		catch (Exception e)
		{
			Debug.Log("<b>PlayerClient: ���� ���� ����!</b> : " + e.Message);
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
		Debug.Log("PlayerClient: Disconnect ��û �۽�!");
		
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
		byte[] receivedPacket = new byte[maxPacketSize]; // ��� ũ�� �ϵ��ڵ�
		Span<byte> byteSpan = new Span<byte>(receivedPacket);


		try
		{
			Debug.Log("PlayerClient: ReceiveThread ����!");

			while (bIsConnected)
			{
				byteSpan.Fill(0);

				int recvLength = CVSPv2.Recv(socket, ref outCommand, ref outOption, ref receivedPacket);

				if (recvLength < 0)
				{
					Debug.LogWarning("ServerConnector: ���! Recv ����: " + recvLength);
					continue;
				}

				//Debug.Log("<b>Receive ���� Command: " + outCommand + ", Option: " + outOption + "</b>");
				//Debug.Log("PlayerClient: recvLength: " + recvLength);


				switch (outCommand)
				{
					// ���� Ʈ������ ����ȭ
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
										//Debug.Log("��Ŀ ������ Ʈ������: " + transformInfo.actorGuid + ", pos: " + observer.netPosition + ", rot: " + observer.netRotation);
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
										//Debug.Log("��Ŀ ������ ������: " + positionInfo.actorGuid + ", pos: " + observer.netPosition);
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
										//Debug.Log("��Ŀ ������ �����̼�: " + rotationInfo.actorGuid + ", rot: " + observer.netRotation);
									}
								}
								break;

							default:
								Debug.LogWarning("SyncActorWorker: �̻��� option�� ����!: " + outOption);
								break;

						}




							break;
					}


					// ä�� ����
					case (CVSPv2.Command.Chatting | CVSPv2.Command.RESPONSE):
					{
						string message = Serializer.DeserializeEucKrString(receivedPacket, recvLength);
						Debug.Log("ServerConnector: �����κ��� ä�� ����! : " + message);

						ChattingInputBox.chattingQueue.Enqueue(message);

						break;
					}


					// ���� ���� ����
					case (CVSPv2.Command.SpawnActor | CVSPv2.Command.RESPONSE):
					{
						Debug.Log("SpawnActor ���� ����!");

						CVSPv2Serializer serializer = new();
						serializer.InterpretSerializedPacket(recvLength, receivedPacket, true);
						ActorSpawnParams spawnParams = new();
						spawnParams.Deserialize(serializer);

						ThreadWorksDistributor.instance.actionsQueue.Enqueue(() => FLGameplayHelper.SpawnActor(spawnParams));

						break;
					}


					// ���Ϳ� ������ ���� ���� �뺸 ����
					case (CVSPv2.Command.ActorNeedToObserved | CVSPv2.Command.RESPONSE):
					{
						CVSPv2Serializer serializer = new();
						serializer.InterpretSerializedPacket(recvLength, receivedPacket, true);

						if (outOption == CVSPv2.Option.Success)	
						{
							Debug.Log("ActorNeedToObserved Success ���� ����! ActorNetObserver�� ���ÿ� ����!");
							TransformInfo transformInfo = new();
							transformInfo.Deserialize(serializer);

							ThreadWorksDistributor.instance.actionsQueue.Enqueue(() => FLGameplayHelper.SpawnActorNetObserver(transformInfo));
						}
						else
						{
							Debug.Log("ActorNeedToObserved Failed ���� ����! ActorNetObserver ����!");
							serializer.Get(out uint observedActorGuid);
							if (actorObservers.TryGetValue(observedActorGuid, out ActorNetObserver actorObserver))
							{
								ThreadWorksDistributor.instance.actionsQueue.Enqueue(() => Destroy(actorObserver.gameObject));
							}
						}

						break;
					}


					// �ſ� ������ ������ RPC
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

						Debug.Log("RPC ���� ����: ���� [" + "]�� ���� �Լ� " + rpcInfo.functionName);

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


					// ���� �α���
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

							Debug.Log("<color=green>�α��� ����! �� ID: " + playerInfo.playerId + ", �г���: " + playerInfo.nickname + "</color>");

							break;
						}

						while (OnLoginFailedCallbackQueue.TryDequeue(out UnityAction dequeued))
						{
							ThreadWorksDistributor.instance.actionsQueue.Enqueue(dequeued);
						}
						Debug.LogWarning("<color=red>�α��� ����!! �г��� Ȥ�� ��й�ȣ�� �߸��Ǿ����ϴ�.</color>");
						

						break;
					}


					// �� ���� ����
					case (CVSPv2.Command.JoinRoom | CVSPv2.Command.RESPONSE):
					{
						if (outOption != CVSPv2.Option.Success)
						{
							Debug.LogWarning("<b>�� ���� ����!</b>");

							if (OnJoinRoomResponseDelegate != null)
								ThreadWorksDistributor.instance.actionsQueue.Enqueue(() => OnJoinRoomResponseDelegate(false, null));
							break;
						}

						Debug.Log("<b>�� ���� ����!</b>");
						CVSPv2Serializer serializer = new();
						serializer.InterpretSerializedPacket(recvLength, receivedPacket, true);


						JoinRoomResponseInfo responseInfo = new();
						responseInfo.Deserialize(serializer);


						Debug.Log("��[" + responseInfo.roomGuid + "] ���� ���� ��� ��...");
						Debug.Log("�� �̸�: " + responseInfo.roomName + ", ���� ID: " + responseInfo.roomMasterPlayerId);
						for (int i = 0; i < 4; ++i)
							Debug.Log("�÷��̾�[" + responseInfo.players[i].playerId + "]�� �г���: " + responseInfo.players[i].nickname);

						if (OnJoinRoomResponseDelegate != null)
							ThreadWorksDistributor.instance.actionsQueue.Enqueue(() => OnJoinRoomResponseDelegate(true, responseInfo));

						break;
					}


					// �� ���� �ҷ�����
					case (CVSPv2.Command.LoadRoomsInfo | CVSPv2.Command.RESPONSE):
					{
						if (outOption != CVSPv2.Option.Success)
						{
							Debug.LogWarning("LoadRoomsInfo ���� ���� ����!");
							if (OnRefreshRoomsListReceivedDelegate != null)
							{
								ThreadWorksDistributor.instance.actionsQueue.Enqueue(() => OnRefreshRoomsListReceivedDelegate(false, null));
							}
							break;
						}

						Debug.Log("LoadRoomsInfo ���� ����!");

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


					// ���� ���� ���� ���ŵ�
					case (CVSPv2.Command.RoomUserRefreshed | CVSPv2.Command.RESPONSE):
					{
						Debug.Log("RoomUserRefreshed ���� ����!");

						if (outOption == CVSPv2.Option.Success)
						{
							CVSPv2Serializer serializer = new();
							serializer.InterpretSerializedPacket(recvLength, receivedPacket, true);


							RoomUserRefreshedInfo refreshedInfo = new();
							refreshedInfo.Deserialize(serializer);

							Debug.Log("RoomUserRefreshed ���: " + refreshedInfo.refreshedUserIndex + "�� �ε����� [" + refreshedInfo.playerInfo.playerId + "], �г���: " + refreshedInfo.playerInfo.nickname);

							if (OnRoomUserRefreshedDelegate != null)
							{
								ThreadWorksDistributor.instance.actionsQueue.Enqueue(() => OnRoomUserRefreshedDelegate(refreshedInfo));
							}
						}
						else if (outOption == CVSPv2.Option.NewRoomMaster)
						{
							Debug.Log("���� ���� ���� ����!");

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
							Debug.LogWarning("RoomUserRefreshed���� �� �� ���� �ɼ��� ����!");
						}
						

						break;
					}


					// �� ������
					case (CVSPv2.Command.LeaveRoom | CVSPv2.Command.RESPONSE):
					{
						if (outOption == CVSPv2.Option.Success)
						{
							Debug.Log("LeaveRoom Success ����!");

							while (OnLeaveRoomSuccessedCallbackQueue.TryDequeue(out UnityAction dequeued))
							{
								ThreadWorksDistributor.instance.actionsQueue.Enqueue(dequeued);
							}
						}

						break;
					}


					// ���� ����
					case (CVSPv2.Command.StartGame | CVSPv2.Command.RESPONSE):
					{
						Debug.Log("StartGame ����!");

						if (outOption == CVSPv2.Option.Success)
						{
							bIsInGame = true;
							ThreadWorksDistributor.instance.actionsQueue.Enqueue(() => SceneManager.LoadScene("GameScene"));
						}

						break;
					}


					// ���� �� ���� �ε� ����! (���� �� �ε� ���� ����ȭ)
					case (CVSPv2.Command.PlayerHasLoadedGameScene | CVSPv2.Command.RESPONSE):
					{
						Debug.Log("PlayerHasLoadedGameScene ����!");

						
						CVSPv2Serializer serializer = new();
						serializer.InterpretSerializedPacket(recvLength, receivedPacket, true);
						LoadSceneCompleteInfo info = new();
						info.Deserialize(serializer);


						if (outOption == CVSPv2.Option.Success)
						{
							Debug.Log("<color=green><b>[" + info.playerId + "]�� �� �ε� ����!</b></color>");
							Debug.Log("<color=green><b>���� �ε� ��Ȳ: [ " 
								+ info.currentLoadedPlayerCount + " / " + info.validPlayerCount + " ]</b></color>");
						}
						else
						{
							Debug.Log("<color=orange><b>[" + info.playerId + "]�� �� �ε� ����!</b></color>");
							Debug.Log("<color=green><b>���� �ε� ��Ȳ: [ " 
								+ info.currentLoadedPlayerCount + " / " + info.validPlayerCount + " ]</b></color>");
						}


						break;
					}


					// ��� ������ �ε��� ����! �� �����κ��� ���۹���!
					case (CVSPv2.Command.AllPlayersInRoomHaveFinishedLoading | CVSPv2.Command.RESPONSE):
					{
						Debug.Log("AllPlayersInRoomHaveFinishedLoading ����!");

						if (outOption != CVSPv2.Option.Success)
						{
							Debug.LogWarning("<b><color=red>AllPlayersInRoomHaveFinishedLoading ���� ���� ����!!</b></color>");
							break;
						}

						Debug.Log("PlayerController ���� ���� ��û!");
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
			Debug.LogWarning("ServerConnector: ������ ���� ������ ���� ����! �޽���: " + e.Message);
			Debug.LogWarning("ȣ�� ����: " + e.StackTrace);
			Debug.LogWarning("Data: " + e.Data);

			OnDisconnected();

			return;
		}

		Debug.Log("PlayerClient: ReceiveThread ����!");
	}
}

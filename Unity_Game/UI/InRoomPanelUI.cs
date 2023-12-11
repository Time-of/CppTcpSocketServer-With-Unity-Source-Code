using CVSP;
using System.Collections;
using System.Collections.Generic;
using Unity.VisualScripting;
using UnityEngine;
using UnityEngine.Events;
using UnityEngine.UI;
using static UnityEngine.AdaptivePerformance.Provider.AdaptivePerformanceSubsystemDescriptor;


/// <summary>
/// ���� �����ִ� �� ������ �����ϴ� �г� UI
/// </summary>
public class InRoomPanelUI : MonoBehaviour
{
	public List<RoomUserInfoUI> roomUserInfoUIs = new();

	public Button startGameButton;
	public Button leaveLobbyButton;

	public UnityAction OnLeaveSuccessedDelegate;


	public void Start()
	{
		if (roomUserInfoUIs.Count != 4)
		{
			Debug.LogAssertion("roomUserInfoUIs�� 4���� �ƴ�!");
		}

		startGameButton.onClick.AddListener(RequestStartGame);
		leaveLobbyButton.onClick.AddListener(RequestLeaveLobby);
	}


	#region ���� ����
	public void UpdateInfo(List<PlayerInfo> playerInfos)
	{
		if (playerInfos.Count != 4)
		{
			Debug.LogAssertion("playerInfos�� ũ�Ⱑ 4�� �ƴ�! ũ��: " + playerInfos.Count);
		}

		for (int i = 0; i < 4; ++i)
		{
			roomUserInfoUIs[i].UpdateInfo(playerInfos[i]);
		}
	}


	public void UpdateRoomMaster(int newRoomMasterPlayerId)
	{
		Debug.Log("�� ���� ID: " + newRoomMasterPlayerId);

		bool bFound = false;
		for (int i = 0; i < 4; ++i)
		{
			if (roomUserInfoUIs[i].info.playerId == newRoomMasterPlayerId)
			{
				roomUserInfoUIs[i].ShowRoomMasterImage();
				bFound = true;
				break;
			}
		}

		if (!bFound && !gameObject.activeInHierarchy)
		{
			delayFindRoomMaster = newRoomMasterPlayerId;
		}
	}


	int delayFindRoomMaster = -1;
	private void OnEnable()
	{
		if (delayFindRoomMaster != -1)
		{
			DelayUpdateRoomMasterUntilEnabled(delayFindRoomMaster);
		}
	}


	// OnEnabled�� �� ������ ���
	private void DelayUpdateRoomMasterUntilEnabled(int newRoomMasterPlayerId)
	{
		for (int k = 0; k < 4; ++k)
		{
			if (roomUserInfoUIs[k].info.playerId == newRoomMasterPlayerId)
			{
				roomUserInfoUIs[k].ShowRoomMasterImage();
				Debug.Log("���� �߰�! �ε���: " + k);
				Debug.Log("�� ���� ����. ID: " + newRoomMasterPlayerId);
				return;
			}
		}

		Debug.LogWarning("OnEnable()������ ������ ã�� �� ������!!!");
	}


	public void RefreshUser(RoomUserRefreshedInfo refreshedInfo)
	{
		if (refreshedInfo == null || refreshedInfo.refreshedUserIndex > 5)
		{
			Debug.LogWarning("RefreshUser ��û�� �޾����� ��ȿ���� ���� ������!");
			return;
		}

		Debug.Log("���� ���� ���� ����! �ε���: " + refreshedInfo.refreshedUserIndex 
			+ ", id: " + refreshedInfo.playerInfo.playerId 
			+ ", �г���: " + refreshedInfo.playerInfo.nickname);

		if (refreshedInfo.playerInfo == null) refreshedInfo.playerInfo = new PlayerInfo();

		roomUserInfoUIs[refreshedInfo.refreshedUserIndex].UpdateInfo(refreshedInfo.playerInfo);
	}
	#endregion


	#region ���� ����
	private void RequestStartGame()
	{
		PlayerClient.instance.Send(CVSPv2.Command.StartGame);
	}
	#endregion


	private void RequestLeaveLobby()
	{
		PlayerClient.instance.OnLeaveRoomSuccessedCallbackQueue.Enqueue(ReturnToLobby);
		PlayerClient.instance.Send(CVSPv2.Command.LeaveRoom);
	}


	private void ReturnToLobby()
	{
		if (OnLeaveSuccessedDelegate != null)
			OnLeaveSuccessedDelegate.Invoke();
	}
}

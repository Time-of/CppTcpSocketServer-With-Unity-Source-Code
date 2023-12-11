using CVSP;
using System.Collections;
using System.Collections.Generic;
using Unity.VisualScripting;
using UnityEngine;
using UnityEngine.Events;
using UnityEngine.UI;
using static UnityEngine.AdaptivePerformance.Provider.AdaptivePerformanceSubsystemDescriptor;


/// <summary>
/// 현재 들어와있는 방 정보를 관리하는 패널 UI
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
			Debug.LogAssertion("roomUserInfoUIs가 4개가 아님!");
		}

		startGameButton.onClick.AddListener(RequestStartGame);
		leaveLobbyButton.onClick.AddListener(RequestLeaveLobby);
	}


	#region 유저 정보
	public void UpdateInfo(List<PlayerInfo> playerInfos)
	{
		if (playerInfos.Count != 4)
		{
			Debug.LogAssertion("playerInfos의 크기가 4가 아님! 크기: " + playerInfos.Count);
		}

		for (int i = 0; i < 4; ++i)
		{
			roomUserInfoUIs[i].UpdateInfo(playerInfos[i]);
		}
	}


	public void UpdateRoomMaster(int newRoomMasterPlayerId)
	{
		Debug.Log("새 방장 ID: " + newRoomMasterPlayerId);

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


	// OnEnabled가 될 때까지 대기
	private void DelayUpdateRoomMasterUntilEnabled(int newRoomMasterPlayerId)
	{
		for (int k = 0; k < 4; ++k)
		{
			if (roomUserInfoUIs[k].info.playerId == newRoomMasterPlayerId)
			{
				roomUserInfoUIs[k].ShowRoomMasterImage();
				Debug.Log("방장 발견! 인덱스: " + k);
				Debug.Log("새 방장 설정. ID: " + newRoomMasterPlayerId);
				return;
			}
		}

		Debug.LogWarning("OnEnable()에서도 방장을 찾을 수 없었음!!!");
	}


	public void RefreshUser(RoomUserRefreshedInfo refreshedInfo)
	{
		if (refreshedInfo == null || refreshedInfo.refreshedUserIndex > 5)
		{
			Debug.LogWarning("RefreshUser 요청을 받았으나 유효하지 않은 정보임!");
			return;
		}

		Debug.Log("방의 유저 정보 갱신! 인덱스: " + refreshedInfo.refreshedUserIndex 
			+ ", id: " + refreshedInfo.playerInfo.playerId 
			+ ", 닉네임: " + refreshedInfo.playerInfo.nickname);

		if (refreshedInfo.playerInfo == null) refreshedInfo.playerInfo = new PlayerInfo();

		roomUserInfoUIs[refreshedInfo.refreshedUserIndex].UpdateInfo(refreshedInfo.playerInfo);
	}
	#endregion


	#region 게임 시작
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

using CVSP;
using System.Collections;
using System.Collections.Generic;
using TMPro;
using UnityEngine;
using UnityEngine.UI;


/// <summary>
/// 로비 화면에 보이는 방 정보 패널
/// </summary>
public class LobbyRoomInfoPanel : MonoBehaviour
{
	public TMP_Text currentPlayersCount;
	public uint roomId = 0;
	public TMP_Text roomIdText;
	public TMP_Text roomName;
	public GameObject selectedImage;
	public Button roomButton;
	RoomManager roomManager;


	public void Start()
	{
		roomButton.onClick.AddListener(Select);
	}


	public void UpdateInfo(RoomManager roomManager, RoomInfo roomInfo)
	{
		this.roomManager = roomManager;
		roomManager.OnLobbyRoomInfoPanelsBeginRefreshDelegate += OnRefreshButtonClicked;
		currentPlayersCount.text = roomInfo.currentPlayerCount.ToString();
		roomId = roomInfo.roomGuid;
		roomIdText.text = roomId.ToString();
		roomName.text = roomInfo.roomName;
	}


	public void Select()
	{
		selectedImage.SetActive(true);
		roomManager.RoomSelected(this);
	}


	public void Unselect()
	{
		selectedImage.SetActive(false);
	}


	private void OnRefreshButtonClicked()
	{
		if (gameObject == null) return;

		roomManager.OnLobbyRoomInfoPanelsBeginRefreshDelegate -= OnRefreshButtonClicked;
		Destroy(gameObject);
	}
}

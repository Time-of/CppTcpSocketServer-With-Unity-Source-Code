using CVSP;
using System.Collections;
using System.Collections.Generic;
using TMPro;
using UnityEngine;
using UnityEngine.UI;


/// <summary>
/// 방의 플레이어 정보 UI 클래스입니다.
/// </summary>
public class RoomUserInfoUI : MonoBehaviour
{
	public PlayerInfo info = new();
	public TMP_Text playerIdText;
	public TMP_Text playerNicknameText;
	public Image roomMasterImage;


	public void UpdateInfo(PlayerInfo playerInfo)
	{
		info = playerInfo;

		if (info.playerId == -1)
		{
			playerIdText.text = "플레이어 없음";
		}
		else
		{
			playerIdText.text = "ID: " + info.playerId.ToString();
		}
		
		playerNicknameText.text = info.nickname;
		roomMasterImage.gameObject.SetActive(false);
	}


	public void ShowRoomMasterImage()
	{
		roomMasterImage.gameObject.SetActive(true);
	}
}

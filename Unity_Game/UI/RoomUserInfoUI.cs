using CVSP;
using System.Collections;
using System.Collections.Generic;
using TMPro;
using UnityEngine;
using UnityEngine.UI;


/// <summary>
/// ���� �÷��̾� ���� UI Ŭ�����Դϴ�.
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
			playerIdText.text = "�÷��̾� ����";
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

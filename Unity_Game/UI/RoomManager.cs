using CVSP;
using System.Collections;
using System.Collections.Generic;
using TMPro;
using UnityEngine;
using UnityEngine.Events;
using UnityEngine.UI;


public enum ERoomManageState
{
	NONE, IN_LOBBY, CREATE_ROOM, IN_ROOM, LOADING
}


/// <summary>
/// �� ���� Ŭ����.
/// ���� �� public���� �Ǿ� �ִ� �� ���� �ð��� �����ؼ� �׷����ϴ�....
/// </summary>
public class RoomManager : MonoBehaviour
{
	[Header("���� �г� ������...")]
	public GameObject lobbyPanel;
	public GameObject createRoomPanel;
	private GameObject panelNowShowing = null;
	private ERoomManageState state = ERoomManageState.NONE;
	public ChattingInputBox chatting;

	public GameObject loadingImage;
	private ERoomManageState prevStateBeforeLoading = ERoomManageState.NONE;


	// �� Ž�� â
	[Space(2)]
	[Header("�� Ž�� â")]
	public Button lobbyCreateRoomButton;
	public Button lobbyJoinRoomButton;
	public Button lobbyRefreshRoomsButton;
	[SerializeField] private uint selectedRoomGuid = 0;
	public TMP_Text lobbySelectedRoomNameText;
	public VerticalLayoutGroup lobbyRoomInfosVerticalGroup;


	// �� ����� â
	[Space(2)]
	[Header("�� ����� â")]
	public TMP_InputField createRoomNameInputField;
	public Button confirmCreateRoomButton;
	public Button cancelCreateRoomButton;


	// �� ���� â
	[Space(2)]
	[Header("�� ���� â")]
	public InRoomPanelUI inRoomPanelUI;


	// �� ��
	[Space(2)]
	[Header("�� ��...")]
	public LobbyRoomInfoPanel lobbyRoomInfoPanelPrefab;
	public UnityAction OnLobbyRoomInfoPanelsBeginRefreshDelegate;
	private bool bCanRefreshRoomsOnLobby = false;


	private void Start()
	{
		lobbyPanel.SetActive(false);
		createRoomPanel.SetActive(false);
		panelNowShowing = null;
		lobbySelectedRoomNameText.text = "[�� ���õ��� ����]";
		PlayerClient.instance.OnLoginSuccessedCallbackQueue.Enqueue(OnLoginSuccessed);
		PlayerClient.instance.OnRoomMasterUpdatedDelegate += inRoomPanelUI.UpdateRoomMaster;
		inRoomPanelUI.OnLeaveSuccessedDelegate += ReturnToLobby;
	}


	private void OnLoginSuccessed()
	{
		bCanRefreshRoomsOnLobby = true;
		ChangeState(ERoomManageState.IN_LOBBY);
		RequestRoomsInformation();
	}


	#region �κ�
	private void RequestRoomsInformation()
	{
		RequestRoomsInformation("[�� ���õ��� ����]");
	}


	private void RequestRoomsInformation(string selectedRoomNameText)
	{
		selectedPanel = null;
		selectedRoomGuid = 0;
		lobbySelectedRoomNameText.text = selectedRoomNameText;
		Debug.Log("RoomManager: �� ���� ���ΰ�ħ ��û �õ�...");

		if (!bCanRefreshRoomsOnLobby) 
		{
			Debug.Log("RoomManager: �� ���� ���ΰ�ħ ��û ����! ���� ���ΰ�ħ�� �� ����!: " + bCanRefreshRoomsOnLobby);
			return;
		}
		bCanRefreshRoomsOnLobby = false;

		StartCoroutine(RequestRefreshRoomsCoroutine());
	}


	private IEnumerator RequestRefreshRoomsCoroutine()
	{
		BeginLoading();
		PlayerClient.instance.OnRefreshRoomsListReceivedDelegate += RefreshRooms;

		yield return new WaitForSeconds(0.1f);

		PlayerClient.instance.Send(CVSPv2.Command.LoadRoomsInfo);
	}


	private void RefreshRooms(bool bResult, RoomInfoList roomInfoList)
	{
		PlayerClient.instance.OnRefreshRoomsListReceivedDelegate -= RefreshRooms;

		if (OnLobbyRoomInfoPanelsBeginRefreshDelegate != null)
			OnLobbyRoomInfoPanelsBeginRefreshDelegate.Invoke();

		StartCoroutine(RefreshRoomsCoroutine(bResult, roomInfoList));
	}


	private IEnumerator RefreshRoomsCoroutine(bool bResult, RoomInfoList roomInfoList)
	{
		if (bResult)
		{
			Debug.Log("<color=green>RoomManager: �� ���� ���ΰ�ħ ����!</color>");
			foreach (var room in roomInfoList.rooms)
			{
				Debug.Log("�� [" + room.roomGuid + "] [" + room.roomName + "] " + room.currentPlayerCount + " / " + "4");

				var createdPanel = Instantiate(lobbyRoomInfoPanelPrefab, lobbyRoomInfosVerticalGroup.transform);
				createdPanel.UpdateInfo(this, room);
			}
		}
		

		yield return new WaitForSeconds(1.0f);

		EndLoading();
		bCanRefreshRoomsOnLobby = true;
	}


	private LobbyRoomInfoPanel selectedPanel = null;
	public void RoomSelected(LobbyRoomInfoPanel panel)
	{
		if (selectedPanel != null) selectedPanel.Unselect();
		if (panel == null)
		{
			selectedPanel = null;
			return;
		}

		selectedRoomGuid = panel.roomId;
		selectedPanel = panel;
		lobbySelectedRoomNameText.text = "[" + panel.roomName.text + "]";
	}
	

	private void OnCreateRoomButtonClicked()
	{
		ChangeState(ERoomManageState.CREATE_ROOM);
	}


	private void OnJoinRoomButtonClicked()
	{
		if (selectedRoomGuid == 0)
		{
			Debug.Log("RoomManager: ���õ� �� ����!");
			return;
		}

		StartCoroutine(JoinRoomCoroutine());
	}


	private IEnumerator JoinRoomCoroutine()
	{
		BeginLoading();
		Debug.Log("RoomManager: ���õ� �� " + selectedRoomGuid + "�� ���� ��û...");

		PlayerClient.instance.OnJoinRoomResponseDelegate += OnJoinRoomResponseReceived;

		yield return new WaitForSeconds(0.25f);

		CVSPv2Serializer serializer = new();
		JoinRoomRequestInfo info = new(selectedRoomGuid);
		serializer.Push(info);
		int result = CVSPv2.SendSerializedPacket(PlayerClient.instance.GetSocket(), CVSPv2.Command.JoinRoom, CVSPv2.Option.Success, serializer);
		if (result <= 0)
		{
			PlayerClient.instance.OnJoinRoomResponseDelegate -= OnJoinRoomResponseReceived;
			EndLoading();
		}
	}


	private void OnJoinRoomResponseReceived(bool bResult, JoinRoomResponseInfo info)
	{
		Debug.Log("RoomManager: �� Join ���� ���: " + bResult);
		if (!bResult)
		{
			PlayerClient.instance.OnJoinRoomResponseDelegate -= OnJoinRoomResponseReceived;
			EndLoading();
			RequestRoomsInformation("<color=red><b>�� ���� ����!</b></color>");
			return;
		}

		
		// �� ����
		UpdateRoomPlayersInfoOnJoinedRoom(info.players, info.roomMasterPlayerId);
		ChangeState(ERoomManageState.IN_ROOM);
	}
	#endregion


	#region �� ���� â
	private void OnConfirmCreateRoomButtonClicked()
	{
		if (!gameObject.activeInHierarchy || createRoomNameInputField.text.Length < 2 || createRoomNameInputField.text.Length > 30) return;

		StartCoroutine(CreateRoomCoroutine());
	}


	// ��Ȥ ���� ������ �̺�Ʈ ��Ϻ��� ������ �Ͼ�� �ڷ�ƾ�� ���...
	private IEnumerator CreateRoomCoroutine()
	{
		BeginLoading();
		PlayerClient.instance.OnJoinRoomResponseDelegate += OnJoinRoomResponseReceived;

		yield return new WaitForSeconds(0.2f);

		CVSPv2Serializer serializer = new();
		CreateRoomInfo info = new(createRoomNameInputField.text);
		serializer.Push(info);
		int result = CVSPv2.SendSerializedPacket(PlayerClient.instance.GetSocket(), CVSPv2.Command.CreateRoom, CVSPv2.Option.Success, serializer);
		if (result <= 0)
		{
			Debug.LogError("OnConfirmCreateRoomButtonClicked: ���� ����: " + result);
			PlayerClient.instance.OnJoinRoomResponseDelegate -= OnJoinRoomResponseReceived;
			EndLoading();
		}
	}


	private void OnCancelCreateRoomButtonClicked()
	{
		ChangeState(ERoomManageState.IN_LOBBY);
	}
	#endregion


	#region �� ���� â
	private void UpdateRoomPlayersInfoOnJoinedRoom(List<PlayerInfo> playerInfos, int roomMasterPlayerId)
	{
		inRoomPanelUI.UpdateInfo(playerInfos);
		inRoomPanelUI.UpdateRoomMaster(roomMasterPlayerId);

		PlayerClient.instance.OnJoinRoomResponseDelegate -= OnJoinRoomResponseReceived;
		EndLoading();
	}


	private void OnRoomUserRefreshedInRoom(RoomUserRefreshedInfo info)
	{
		if (state != ERoomManageState.IN_ROOM)
		{
			Debug.LogWarning("�濡 ���� ������, �� ���� ���� ���� ������ �޾���!");
			return;
		}

		inRoomPanelUI.RefreshUser(info);
	}


	private void ReturnToLobby()
	{
		ChangeState(ERoomManageState.IN_LOBBY);
		RequestRoomsInformation();
	}
	#endregion


	#region ���
	void _ChangePanel(GameObject newPanel, bool bShowChattingPanel)
	{
		chatting.gameObject.SetActive(bShowChattingPanel);
		if (newPanel == null) return;
		if (panelNowShowing != null) panelNowShowing.SetActive(false);
		panelNowShowing = newPanel;
		panelNowShowing.SetActive(true);
	}


	bool ChangeState(ERoomManageState newState)
	{
		// �ε� ���� �� ���� ���� �Ұ�
		if (state == newState || state == ERoomManageState.LOADING) return false;

		switch (state)
		{
			case ERoomManageState.IN_LOBBY:
				lobbyCreateRoomButton.onClick.RemoveListener(OnCreateRoomButtonClicked);
				lobbyJoinRoomButton.onClick.RemoveListener(OnJoinRoomButtonClicked);
				lobbyRefreshRoomsButton.onClick.RemoveListener(RequestRoomsInformation);
				break;

			case ERoomManageState.CREATE_ROOM:
				ThreadWorksDistributor.instance.OnEnterButtonPressedDelegate -= OnConfirmCreateRoomButtonClicked;
				confirmCreateRoomButton.onClick.RemoveListener(OnConfirmCreateRoomButtonClicked);
				cancelCreateRoomButton.onClick.RemoveListener(OnCancelCreateRoomButtonClicked);
				break;

			case ERoomManageState.IN_ROOM:
				PlayerClient.instance.OnRoomUserRefreshedDelegate -= OnRoomUserRefreshedInRoom;
				break;

			default:
				break;
		}

		state = newState;

		switch (state)
		{
			case ERoomManageState.IN_LOBBY:
				_ChangePanel(lobbyPanel, true);
				lobbyCreateRoomButton.onClick.AddListener(OnCreateRoomButtonClicked);
				lobbyJoinRoomButton.onClick.AddListener(OnJoinRoomButtonClicked);
				lobbyRefreshRoomsButton.onClick.AddListener(RequestRoomsInformation);
				break;

			case ERoomManageState.CREATE_ROOM:
				_ChangePanel(createRoomPanel, false);
				ThreadWorksDistributor.instance.OnEnterButtonPressedDelegate += OnConfirmCreateRoomButtonClicked;
				confirmCreateRoomButton.onClick.AddListener(OnConfirmCreateRoomButtonClicked);
				cancelCreateRoomButton.onClick.AddListener(OnCancelCreateRoomButtonClicked);
				createRoomNameInputField.ActivateInputField();
				break;

			case ERoomManageState.IN_ROOM:
				_ChangePanel(inRoomPanelUI.gameObject, true);
				PlayerClient.instance.OnRoomUserRefreshedDelegate += OnRoomUserRefreshedInRoom;
				
				break;

			case ERoomManageState.LOADING:
				loadingImage.SetActive(true);
				break;

			default:
				break;
		}

		Debug.Log("RoomManager state: " + state.ToString());
		return true;
	}


	void BeginLoading()
	{
		prevStateBeforeLoading = state;
		ChangeState(ERoomManageState.LOADING);
	}


	void EndLoading()
	{
		state = ERoomManageState.NONE;
		ChangeState(prevStateBeforeLoading);
		prevStateBeforeLoading = ERoomManageState.NONE;
		loadingImage.SetActive(false);
	}
	#endregion
}

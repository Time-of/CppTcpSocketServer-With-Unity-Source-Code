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
/// 방 관리 클래스.
/// 뭔가 다 public으로 되어 있는 건 개발 시간이 빠듯해서 그렇습니다....
/// </summary>
public class RoomManager : MonoBehaviour
{
	[Header("각종 패널 정보들...")]
	public GameObject lobbyPanel;
	public GameObject createRoomPanel;
	private GameObject panelNowShowing = null;
	private ERoomManageState state = ERoomManageState.NONE;
	public ChattingInputBox chatting;

	public GameObject loadingImage;
	private ERoomManageState prevStateBeforeLoading = ERoomManageState.NONE;


	// 방 탐색 창
	[Space(2)]
	[Header("방 탐색 창")]
	public Button lobbyCreateRoomButton;
	public Button lobbyJoinRoomButton;
	public Button lobbyRefreshRoomsButton;
	[SerializeField] private uint selectedRoomGuid = 0;
	public TMP_Text lobbySelectedRoomNameText;
	public VerticalLayoutGroup lobbyRoomInfosVerticalGroup;


	// 방 만들기 창
	[Space(2)]
	[Header("방 만들기 창")]
	public TMP_InputField createRoomNameInputField;
	public Button confirmCreateRoomButton;
	public Button cancelCreateRoomButton;


	// 방 정보 창
	[Space(2)]
	[Header("방 정보 창")]
	public InRoomPanelUI inRoomPanelUI;


	// 그 외
	[Space(2)]
	[Header("그 외...")]
	public LobbyRoomInfoPanel lobbyRoomInfoPanelPrefab;
	public UnityAction OnLobbyRoomInfoPanelsBeginRefreshDelegate;
	private bool bCanRefreshRoomsOnLobby = false;


	private void Start()
	{
		lobbyPanel.SetActive(false);
		createRoomPanel.SetActive(false);
		panelNowShowing = null;
		lobbySelectedRoomNameText.text = "[방 선택되지 않음]";
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


	#region 로비
	private void RequestRoomsInformation()
	{
		RequestRoomsInformation("[방 선택되지 않음]");
	}


	private void RequestRoomsInformation(string selectedRoomNameText)
	{
		selectedPanel = null;
		selectedRoomGuid = 0;
		lobbySelectedRoomNameText.text = selectedRoomNameText;
		Debug.Log("RoomManager: 방 정보 새로고침 요청 시도...");

		if (!bCanRefreshRoomsOnLobby) 
		{
			Debug.Log("RoomManager: 방 정보 새로고침 요청 실패! 아직 새로고침할 수 없음!: " + bCanRefreshRoomsOnLobby);
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
			Debug.Log("<color=green>RoomManager: 방 정보 새로고침 성공!</color>");
			foreach (var room in roomInfoList.rooms)
			{
				Debug.Log("방 [" + room.roomGuid + "] [" + room.roomName + "] " + room.currentPlayerCount + " / " + "4");

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
			Debug.Log("RoomManager: 선택된 방 없음!");
			return;
		}

		StartCoroutine(JoinRoomCoroutine());
	}


	private IEnumerator JoinRoomCoroutine()
	{
		BeginLoading();
		Debug.Log("RoomManager: 선택된 방 " + selectedRoomGuid + "에 입장 요청...");

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
		Debug.Log("RoomManager: 방 Join 응답 결과: " + bResult);
		if (!bResult)
		{
			PlayerClient.instance.OnJoinRoomResponseDelegate -= OnJoinRoomResponseReceived;
			EndLoading();
			RequestRoomsInformation("<color=red><b>방 입장 실패!</b></color>");
			return;
		}

		
		// 방 입장
		UpdateRoomPlayersInfoOnJoinedRoom(info.players, info.roomMasterPlayerId);
		ChangeState(ERoomManageState.IN_ROOM);
	}
	#endregion


	#region 방 생성 창
	private void OnConfirmCreateRoomButtonClicked()
	{
		if (!gameObject.activeInHierarchy || createRoomNameInputField.text.Length < 2 || createRoomNameInputField.text.Length > 30) return;

		StartCoroutine(CreateRoomCoroutine());
	}


	// 간혹 서버 응답이 이벤트 등록보다 빠르게 일어나서 코루틴을 사용...
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
			Debug.LogError("OnConfirmCreateRoomButtonClicked: 전송 실패: " + result);
			PlayerClient.instance.OnJoinRoomResponseDelegate -= OnJoinRoomResponseReceived;
			EndLoading();
		}
	}


	private void OnCancelCreateRoomButtonClicked()
	{
		ChangeState(ERoomManageState.IN_LOBBY);
	}
	#endregion


	#region 방 정보 창
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
			Debug.LogWarning("방에 있지 않은데, 방 유저 정보 갱신 응답을 받았음!");
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


	#region 기능
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
		// 로딩 중일 땐 상태 변경 불가
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

using CVSP;
using System.Collections;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Net.Sockets;
using System.Text;
using TMPro;
using UnityEngine;
using UnityEngine.UI;



/// <summary>
/// <author>이성수</author>
/// 채팅을 담당하는 인풋 박스 컴포넌트 클래스. <br/>
/// 최초 접속 시 닉네임 입력도 담당합니다. <br/>
/// </summary>
public class ChattingInputBox : MonoBehaviour
{
	[SerializeField]
	private TMP_InputField inputField;

	private bool bWasNicknameEntered = false;
	private bool bInputFieldActivated = false;
	private string localNickname;
	public GameObject passwordInputUi;
	public TMP_InputField passwordInputField;


	[SerializeField]
	private TMP_Text chattingBox;

	[SerializeField]
	private ScrollRect chattingScrollBox;
	
	public static ConcurrentQueue<string> chattingQueue = new();

	private WaitForSeconds chattingPeekDelay = new(0.1f);


	private void Start()
	{
		inputField.ActivateInputField();
		ThreadWorksDistributor.instance.OnEnterButtonPressedDelegate += OnEnterButtonPressed;
		PlayerClient.instance.OnLoginSuccessedCallbackQueue.Enqueue(OnLoginSuccessed); // @todo 나중에 따로 빼기
		PlayerClient.instance.OnLoginFailedCallbackQueue.Enqueue(OnLoginFailed); // @todo 나중에 따로 빼기
	}


	private void OnEnterButtonPressed()
	{
		if (!gameObject.activeInHierarchy) return;

		if (bWasNicknameEntered)
			EnterChat();
		else
			EnterNickname();
	}


	private void EnterNickname()
	{
		if (inputField.text == string.Empty)
		{
			return;
		}


		if (!PlayerClient.instance.bIsConnected)
		{
			// Connect
			bool bResult = PlayerClient.instance.ConnectToServer("127.0.0.1");
			if (!bResult)
			{
				Debug.LogWarning("서버와 연결 실패!");
				return;
			}

			Debug.Log("서버와 연결 성공!");
		}

		Debug.Log("서버에 로그인 요청 전송...");

		StartCoroutine(WaitForServerLoginCoroutine());

		PlayerInfo playerInfo = new();
		localNickname = inputField.text;
		inputField.text = string.Empty;
		playerInfo.nickname = localNickname;
		CVSPv2Serializer serializer = new();
		serializer.Push(playerInfo).Push(passwordInputField.text);
		serializer.QuickSendSerializedAndInitialize(CVSPv2.Command.LoginToServer, CVSPv2.Option.Success, true);
	}


	// @todo 나중에 로그인 클래스 따로 만들어서 빼기.
	bool bLoggedIn = false;
	bool bLoginFailed = false;
	private void OnLoginSuccessed() { bLoggedIn = true; }
	private void OnLoginFailed() { bLoginFailed = true; }
	private IEnumerator WaitForServerLoginCoroutine()
	{
		Debug.Log("로그인 정보 수신 대기 중");
		while (!bLoggedIn || !bLoginFailed)
		{
			yield return new WaitForSeconds(0.2f);
		}

		if (bLoginFailed)
		{
			Debug.Log("로그인 실패로 인해 로그인 정보 수신 대기 종료");
			bLoginFailed = false;
			yield break;
		}

		passwordInputUi.gameObject.SetActive(false);
		GameObject.Find("Placeholder").GetComponent<TMP_Text>().SetText("채팅 입력");

		bWasNicknameEntered = true;
		inputField.DeactivateInputField();
		inputField.gameObject.SetActive(false);
		bInputFieldActivated = false;

		StartPeekingChattingsOnLoginSuccessed();
	}


	private void EnterChat()
	{
		if (bInputFieldActivated)
		{
			inputField.DeactivateInputField();
			inputField.gameObject.SetActive(false);
			bInputFieldActivated = false;
		}
		else
		{
			inputField.gameObject.SetActive(true);
			inputField.ActivateInputField();
			bInputFieldActivated = true;
		}

		if (inputField.text == string.Empty)
		{
			return;
		}

		CVSPv2.SendChat(PlayerClient.instance.GetSocket(), CVSPv2.Command.Chatting, CVSPv2.Option.Success, inputField.text);

		inputField.text = string.Empty;
	}



	// 껐다켰다 하는 UI라서, 채팅 창 게임오브젝트가 꺼졌다 켜지면 채팅 피킹을 멈춤.
	// 따라서 켜질 때 반응하는 기능 추가.
	private void OnEnable()
	{
		if (PlayerClient.instance.bIsConnected) StartCoroutine(PeekChattingMessagesCoroutine());
	}


	private void OnDisable()
	{
		StopCoroutine("PeekChattingMessagesCoroutine");
	}


	private void StartPeekingChattingsOnLoginSuccessed()
	{
		StartCoroutine(PeekChattingMessagesCoroutine());
	}


	public IEnumerator UpdateChattingBoxCoroutine(string message)
	{
		Debug.Log(message);
		chattingBox.text += message;
		yield return null;
		chattingScrollBox.verticalNormalizedPosition = 0.0f;
	}


	private IEnumerator PeekChattingMessagesCoroutine()
	{
		Debug.Log("<b>채팅 메시지 피킹 시작!</b>");

		string receivedMessage;
		while (PlayerClient.instance.bIsConnected)
		{
			if (chattingQueue.TryDequeue(out receivedMessage))
			{
				StartCoroutine(UpdateChattingBoxCoroutine(receivedMessage));
			}

			yield return chattingPeekDelay;
		}

		Debug.Log("<b>채팅 메시지 피킹 종료!</b>");
	}
}

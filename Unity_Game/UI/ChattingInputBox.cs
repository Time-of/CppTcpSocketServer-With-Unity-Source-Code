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
/// <author>�̼���</author>
/// ä���� ����ϴ� ��ǲ �ڽ� ������Ʈ Ŭ����. <br/>
/// ���� ���� �� �г��� �Էµ� ����մϴ�. <br/>
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
		PlayerClient.instance.OnLoginSuccessedCallbackQueue.Enqueue(OnLoginSuccessed); // @todo ���߿� ���� ����
		PlayerClient.instance.OnLoginFailedCallbackQueue.Enqueue(OnLoginFailed); // @todo ���߿� ���� ����
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
				Debug.LogWarning("������ ���� ����!");
				return;
			}

			Debug.Log("������ ���� ����!");
		}

		Debug.Log("������ �α��� ��û ����...");

		StartCoroutine(WaitForServerLoginCoroutine());

		PlayerInfo playerInfo = new();
		localNickname = inputField.text;
		inputField.text = string.Empty;
		playerInfo.nickname = localNickname;
		CVSPv2Serializer serializer = new();
		serializer.Push(playerInfo).Push(passwordInputField.text);
		serializer.QuickSendSerializedAndInitialize(CVSPv2.Command.LoginToServer, CVSPv2.Option.Success, true);
	}


	// @todo ���߿� �α��� Ŭ���� ���� ���� ����.
	bool bLoggedIn = false;
	bool bLoginFailed = false;
	private void OnLoginSuccessed() { bLoggedIn = true; }
	private void OnLoginFailed() { bLoginFailed = true; }
	private IEnumerator WaitForServerLoginCoroutine()
	{
		Debug.Log("�α��� ���� ���� ��� ��");
		while (!bLoggedIn || !bLoginFailed)
		{
			yield return new WaitForSeconds(0.2f);
		}

		if (bLoginFailed)
		{
			Debug.Log("�α��� ���з� ���� �α��� ���� ���� ��� ����");
			bLoginFailed = false;
			yield break;
		}

		passwordInputUi.gameObject.SetActive(false);
		GameObject.Find("Placeholder").GetComponent<TMP_Text>().SetText("ä�� �Է�");

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



	// �����״� �ϴ� UI��, ä�� â ���ӿ�����Ʈ�� ������ ������ ä�� ��ŷ�� ����.
	// ���� ���� �� �����ϴ� ��� �߰�.
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
		Debug.Log("<b>ä�� �޽��� ��ŷ ����!</b>");

		string receivedMessage;
		while (PlayerClient.instance.bIsConnected)
		{
			if (chattingQueue.TryDequeue(out receivedMessage))
			{
				StartCoroutine(UpdateChattingBoxCoroutine(receivedMessage));
			}

			yield return chattingPeekDelay;
		}

		Debug.Log("<b>ä�� �޽��� ��ŷ ����!</b>");
	}
}

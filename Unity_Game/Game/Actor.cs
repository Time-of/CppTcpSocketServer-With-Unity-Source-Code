using CVSP;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;


/// <summary>
/// ���� ������ ��Ʈ��ũ ����ȭ�� �̷������ ��ü��. <br/>
/// ������ ���ؼ��� Resources ������ �־�� ��! <br/>
/// �ַ� ActorNetObserver�� ���� ���õǰų� ������. <br/>
/// </summary>
public class Actor : MonoBehaviour
{
	[SerializeField] protected uint guid = 0;
	[SerializeField] protected int ownerPlayerId = -1;

	public bool bCanBeDamaged = true;
	public float sendSerializeDelay = 0.1f;


	public bool bIsMine { get { return ownerPlayerId == PlayerClient.instance.playerInfo.playerId; } }
	public int ownerId { get { return ownerPlayerId; } }



	[Space(2)]
	[Header("��Ʈ��ŷ - �������")]
	public Vector3 netPosition = Vector3.zero; // ���� �󿡼� Ÿ �÷��̾��� ��ġ ��
	public Quaternion netRotation = Quaternion.identity; // ���� �󿡼� Ÿ �÷��̾��� ȸ�� ��
	public float lastReceivedTime = 0.0f; // �������� ��ġ�� ���������� ���� �ð� (from ActorTuner)
	public Vector3 netVelocity = Vector3.zero;


	public void Initialize(uint newGuid, int newOwnerPlayerId)
	{
		guid = newGuid;
		ownerPlayerId = newOwnerPlayerId;

		Debug.Log("<color=green>���� [" + guid + "] ���� ����! ����: [" + ownerPlayerId + "]</color>");
		OnInitialized();
	}


	protected virtual void OnInitialized() { }


	#region ��Ʈ��ŷ
	// ��Ʈ��ũ ������Ʈ (ActorTuner�� ���� ����)
	public virtual void NetUpdate(float receivedTime)
	{
		float deltaReceived = Mathf.Abs(receivedTime - lastReceivedTime);
		netPosition += netVelocity * deltaReceived;
		netVelocity = netPosition - transform.position;
		transform.position += netVelocity * (1 / sendSerializeDelay);

		transform.rotation = Quaternion.Slerp(transform.rotation, netRotation, Time.deltaTime * 10.0f);
		lastReceivedTime = receivedTime;
	}


	#endregion


	#region ����Ƽ ���
	private void OnDestroy()
	{
		if (bIsMine)
		{
			Debug.Log("���� [" + guid + "] �Ҹ�!!");
			CVSPv2Serializer serializer = new();
			serializer.Push(guid);
			serializer.QuickSendSerializedAndInitialize(CVSPv2.Command.ActorDestroyed, CVSPv2.Option.Success, true);
		}
	}
	#endregion


	public void TakeDamage(float damage)
	{
		if (!bIsMine || !bCanBeDamaged) return;

		/*
		CVSPv2Serializer serializer = new();
		serializer.Push(damage);

		CVSPv2.SendRpcUnsafe("OnDamaged", RPCTarget.AllClients, guid, serializer);
		*/

		CVSPv2Serializer serializer = new();
		RpcInfo info = new()
		{
			actorGuid = guid,
			functionName = "OnDamaged"
		};

		serializer.Push(info).Push(damage);
		serializer.QuickSendSerializedAndInitialize(CVSPv2.Command.Rpc, RPCTarget.AllClients, true);
	}


	public virtual void OnDamaged(float damage) { }
}

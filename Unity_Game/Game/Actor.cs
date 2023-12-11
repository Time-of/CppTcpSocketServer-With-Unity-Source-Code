using CVSP;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;


/// <summary>
/// 게임 씬에서 네트워크 동기화가 이루어지는 객체들. <br/>
/// 스폰을 위해서는 Resources 폴더에 넣어야 함! <br/>
/// 주로 ActorNetObserver에 의해 감시되거나 조종됨. <br/>
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
	[Header("네트워킹 - 비소유자")]
	public Vector3 netPosition = Vector3.zero; // 서버 상에서 타 플레이어의 위치 값
	public Quaternion netRotation = Quaternion.identity; // 서버 상에서 타 플레이어의 회전 값
	public float lastReceivedTime = 0.0f; // 서버에서 위치를 마지막으로 받은 시간 (from ActorTuner)
	public Vector3 netVelocity = Vector3.zero;


	public void Initialize(uint newGuid, int newOwnerPlayerId)
	{
		guid = newGuid;
		ownerPlayerId = newOwnerPlayerId;

		Debug.Log("<color=green>액터 [" + guid + "] 스폰 성공! 오너: [" + ownerPlayerId + "]</color>");
		OnInitialized();
	}


	protected virtual void OnInitialized() { }


	#region 네트워킹
	// 네트워크 업데이트 (ActorTuner에 의해 수행)
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


	#region 유니티 기능
	private void OnDestroy()
	{
		if (bIsMine)
		{
			Debug.Log("액터 [" + guid + "] 소멸!!");
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

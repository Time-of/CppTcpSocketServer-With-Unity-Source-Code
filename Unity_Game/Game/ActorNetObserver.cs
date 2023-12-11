using CVSP;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using UnityEngine;


/// <summary>
/// 액터의 움직임을 파악하여 서버에 전송하거나, 
/// 서버에서 받아온 타 플레이어의 움직임 데이터를 액터에 
/// 적용하여, 서버와 데이터를 동기화하는 역할의 클래스입니다. <br/>
/// <br/>
/// 
/// 주 역할은 소유권에 따라 다릅니다. <br/>
/// - 본인 소유라면, 감시 역할이며 <br/>
/// - 타인 소유라면, 조종 역할입니다. <br/>
/// <br/>
/// 
/// 액터에 의해서 서버로부터 생성됨! (서버가 판단하여 생성/소멸 결정)
/// PlayerClient가 관리.
/// </summary>
public class ActorNetObserver : MonoBehaviour
{
	private CVSPv2Serializer serializer = new();
	[SerializeField] private Actor actor = null;
	public uint actorGuid = 0;
	private int ownerPlayerId = -1;

	public bool bIsMine { get { return ownerPlayerId == PlayerClient.instance.playerInfo.playerId; } }


	[Space(2)]
	[Header("네트워킹 - 소유자")]
	[SerializeField] private Vector3 lastPosition = Vector3.zero;
	[SerializeField] private Quaternion lastRotation = Quaternion.identity;
	[SerializeField] private byte transformFlags = 0; // 1: 위치, 2: 회전, 4: 둘 중 하나라도 발생
	public float lastSendTime = 0.0f;


	[Space(2)]
	[Header("네트워킹 - 비소유자")]
	public Vector3 netPosition = Vector3.zero; // 서버 상에서 타 플레이어의 위치 값
	public Quaternion netRotation = Quaternion.identity; // 서버 상에서 타 플레이어의 회전 값
	[SerializeField] private float netReceivedTime = 0.0f; // 서버에서 업데이트받은 시간 (로컬 계산)


	public void Initialize(Actor myActor, uint actorGuid, int ownerPlayerId, Vector3 netPos, Quaternion netRot)
	{
		this.actorGuid = actorGuid;
		this.ownerPlayerId = ownerPlayerId;
		actor = myActor;

		if (actor == null) return;
		netPosition = actor.transform.position;
		netRotation = actor.transform.rotation;

		Debug.Log("ActorNetObserver [" + actorGuid + "] 초기화 완료!");

		name = "[Ob]" + actor.name;
	}


	public void Update()
	{
		if (actor == null || ownerPlayerId == -1)
		{
			return;
		}

		transformFlags = 0b0000;


		// 본인의 것이 아닌 경우
		if (!bIsMine)
		{
			_CheckPositionFlag(lastPosition, netPosition);
			_CheckRotationFlag(lastRotation, netRotation);

			if ((transformFlags & 0b0100) != 0)
			{
				actor.netPosition = netPosition;
				actor.netRotation = netRotation;
				netReceivedTime = Time.time;
			}

			actor.NetUpdate(netReceivedTime);

			if ((transformFlags & 0b0100) != 0)
			{
				lastPosition = netPosition;
				lastRotation = netRotation;
			}
			return;
		}

		if (lastSendTime + actor.sendSerializeDelay > Time.time) return;
		lastSendTime = Time.time + actor.sendSerializeDelay;

		// 내 것인 경우
		_CheckPositionFlag(actor.transform.position, lastPosition);
		_CheckRotationFlag(actor.transform.rotation, lastRotation);

		serializer.Initialize();

		// 둘 중 하나의 변화라도 있었다면 직렬화해서 전송
		if ((transformFlags & 0b0100) != 0)
		{
			serializer.Push(actorGuid);

			byte option = CVSPv2.Option.Failed;

			if ((transformFlags & 0b0001) != 0)
			{
				serializer.Push(actor.transform.position.GetUnityVector3());
				option = CVSPv2.Option.SyncActorPositionOnly;
			}

			if ((transformFlags & 0b0010) != 0)
			{
				serializer.Push(actor.transform.rotation.GetUnityQuaternion());
				option = (option == CVSPv2.Option.SyncActorPositionOnly) ? CVSPv2.Option.Success : CVSPv2.Option.SyncActorRotationOnly;
			}

			serializer.QuickSendSerializedAndInitialize(CVSPv2.Command.SyncActorTransform, option, false);
		}


		lastPosition = actor.transform.position;
		lastRotation = actor.transform.rotation;
	}


	[MethodImpl(MethodImplOptions.AggressiveInlining)] // C++의 inline 키워드와 유사
	private void _CheckPositionFlag(Vector3 source, Vector3 newPos)
	{
		if (!NearlyEqual(source, newPos))
		{
			transformFlags |= 0b0101;
		}
	}


	[MethodImpl(MethodImplOptions.AggressiveInlining)] // C++의 inline 키워드와 유사
	private void _CheckRotationFlag(Quaternion source, Quaternion newRot)
	{
		if (source != newRot)
		{
			transformFlags |= 0b0110;
		}
	}


	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	private bool NearlyEqual(Vector3 lhs, Vector3 rhs)
	{
		float num = lhs.x - rhs.x;
		float num2 = lhs.y - rhs.y;
		float num3 = lhs.z - rhs.z;
		float num4 = num * num + num2 * num2 + num3 * num3;
		return num4 < 0.001f;
	}


	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	private bool NearlyEqual(Quaternion lhs, Quaternion rhs)
	{
		return Quaternion.Dot(lhs, rhs) > 0.999f;
	}


	private void OnDestroy()
	{
		FLGameplayHelper.actorDictionary.Remove(actorGuid);
		PlayerClient.actorObservers.Remove(actorGuid);
		Debug.Log("ActorNetObserver [" + actorGuid + "] 제거됨!");
  		if (actor != null) Destroy(actor);
	}
}

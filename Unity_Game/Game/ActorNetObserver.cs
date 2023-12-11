using CVSP;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using UnityEngine;


/// <summary>
/// ������ �������� �ľ��Ͽ� ������ �����ϰų�, 
/// �������� �޾ƿ� Ÿ �÷��̾��� ������ �����͸� ���Ϳ� 
/// �����Ͽ�, ������ �����͸� ����ȭ�ϴ� ������ Ŭ�����Դϴ�. <br/>
/// <br/>
/// 
/// �� ������ �����ǿ� ���� �ٸ��ϴ�. <br/>
/// - ���� �������, ���� �����̸� <br/>
/// - Ÿ�� �������, ���� �����Դϴ�. <br/>
/// <br/>
/// 
/// ���Ϳ� ���ؼ� �����κ��� ������! (������ �Ǵ��Ͽ� ����/�Ҹ� ����)
/// PlayerClient�� ����.
/// </summary>
public class ActorNetObserver : MonoBehaviour
{
	private CVSPv2Serializer serializer = new();
	[SerializeField] private Actor actor = null;
	public uint actorGuid = 0;
	private int ownerPlayerId = -1;

	public bool bIsMine { get { return ownerPlayerId == PlayerClient.instance.playerInfo.playerId; } }


	[Space(2)]
	[Header("��Ʈ��ŷ - ������")]
	[SerializeField] private Vector3 lastPosition = Vector3.zero;
	[SerializeField] private Quaternion lastRotation = Quaternion.identity;
	[SerializeField] private byte transformFlags = 0; // 1: ��ġ, 2: ȸ��, 4: �� �� �ϳ��� �߻�
	public float lastSendTime = 0.0f;


	[Space(2)]
	[Header("��Ʈ��ŷ - �������")]
	public Vector3 netPosition = Vector3.zero; // ���� �󿡼� Ÿ �÷��̾��� ��ġ ��
	public Quaternion netRotation = Quaternion.identity; // ���� �󿡼� Ÿ �÷��̾��� ȸ�� ��
	[SerializeField] private float netReceivedTime = 0.0f; // �������� ������Ʈ���� �ð� (���� ���)


	public void Initialize(Actor myActor, uint actorGuid, int ownerPlayerId, Vector3 netPos, Quaternion netRot)
	{
		this.actorGuid = actorGuid;
		this.ownerPlayerId = ownerPlayerId;
		actor = myActor;

		if (actor == null) return;
		netPosition = actor.transform.position;
		netRotation = actor.transform.rotation;

		Debug.Log("ActorNetObserver [" + actorGuid + "] �ʱ�ȭ �Ϸ�!");

		name = "[Ob]" + actor.name;
	}


	public void Update()
	{
		if (actor == null || ownerPlayerId == -1)
		{
			return;
		}

		transformFlags = 0b0000;


		// ������ ���� �ƴ� ���
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

		// �� ���� ���
		_CheckPositionFlag(actor.transform.position, lastPosition);
		_CheckRotationFlag(actor.transform.rotation, lastRotation);

		serializer.Initialize();

		// �� �� �ϳ��� ��ȭ�� �־��ٸ� ����ȭ�ؼ� ����
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


	[MethodImpl(MethodImplOptions.AggressiveInlining)] // C++�� inline Ű����� ����
	private void _CheckPositionFlag(Vector3 source, Vector3 newPos)
	{
		if (!NearlyEqual(source, newPos))
		{
			transformFlags |= 0b0101;
		}
	}


	[MethodImpl(MethodImplOptions.AggressiveInlining)] // C++�� inline Ű����� ����
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
		Debug.Log("ActorNetObserver [" + actorGuid + "] ���ŵ�!");
	}
}

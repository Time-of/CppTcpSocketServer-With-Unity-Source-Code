using CVSP;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;


// 옛날 코드 주워다 짜맞추기...
// 남은 시간이 많지 않아서... 옛날에 작성했던 코드를 복사하여
// 적당히 돌아가게끔만 붙여넣은 누더기 골렘 같은 코드...
public class BossSkeleton : Actor
{
	public Animator animator;
	public float health = 100.0f;

	public Image healthbar;
	public bool bIsDead = false;
	public LayerMask friendlyLayer;
	public float aggroDistance = 20.0f;
	public Actor targetActor;


	private void Start()
	{
		bCanBeDamaged = true;
		friendlyLayer = 1 << LayerMask.NameToLayer("Friendly");
	}


	protected override void OnInitialized()
	{
		bCanAttack = true;

		if (bIsMine)
			StartCoroutine(CheckAggroCoroutine());
	}


	private void OnDrawGizmos()
	{
		Gizmos.color = Color.red;
		Gizmos.DrawWireSphere(transform.position, aggroDistance);
	}


	public override void OnDamaged(float damage)
	{
		if (bIsDead) return;

		health -= damage;
		if (health <= 0.0f)
		{
			bIsDead = true;
			StopAllCoroutines();
			animator.SetTrigger("Die");
		}
	}


	private void Update()
	{
		if (bIsDead || !bIsMine) return;

		Look();
	}


	public bool bCanAttack = true;


	private void Look()
	{
		if (targetActor != null && bCanAttack)
		{
			Quaternion lookRot = Quaternion.LookRotation(targetActor.transform.position - transform.position);
			float yRotation = Quaternion.RotateTowards(transform.rotation, lookRot, 80.0f * Time.deltaTime).eulerAngles.y;
			transform.rotation = Quaternion.Euler(0, yRotation, 0);
		}
	}


	protected void TryAttack()
	{
		if (targetActor != null)
		{
			if (Vector3.Angle(transform.forward, (targetActor.transform.position - transform.position).normalized) <= 20.0f)
			{
				if (bCanAttack)
				{
					Debug.Log("공격!");
					bCanAttack = false;
					int randomAttack = Random.Range(0, 100) % 4;
					CVSPv2Serializer serializer = new();
					RpcInfo info = new();
				
					info.actorGuid = guid;
					info.functionName = "PerformAttack";
					
					serializer.Push(info).Push(randomAttack);
					serializer.QuickSendSerializedAndInitialize(CVSPv2.Command.Rpc, RPCTarget.AllClients, true);
				}
			}
		}
	}

	public void PerformAttack(int randomAttack)
	{
		if (bIsDead) return;

		bCanAttack = false;
		
		animator.SetInteger("AttackNumber", randomAttack);
		animator.SetTrigger("isAttack");

		float patternTime;

		switch (randomAttack)
		{
			case 0:
				patternTime = 6f;
				break;
			case 1:
				patternTime = 7f;
				break;
			case 2:
				patternTime = 6f;
				break;
			case 3:
				patternTime = 6.4f;
				break;
			default:
				patternTime = 1f;
				break;
		}

		StartCoroutine(PatternDelayCoroutine(patternTime));
	}


	IEnumerator PatternDelayCoroutine(float time)
	{
		yield return new WaitForSeconds(time);

		bCanAttack = true;
	}


	IEnumerator CheckAggroCoroutine()
	{
		while (!bIsDead)
		{
			if (bCanAttack)
			{
				int count = 0;
				float min_distance = 100000;

				foreach (Collider i in Physics.OverlapSphere(transform.position, aggroDistance, friendlyLayer))
				{
					Actor target = i.GetComponent<Actor>();
					count++;

					if (target != null)
					{
						if (!target.bCanBeDamaged) continue;

						if (min_distance > Vector3.SqrMagnitude(i.transform.position - transform.position))
						{
							min_distance = Vector3.SqrMagnitude(i.transform.position - transform.position);
							targetActor = target;
						}
					}
				}

				if (count == 0)
				{
					targetActor = null;
				}

				TryAttack();
			}

			yield return new WaitForSeconds(0.2f);
		}
	}
}

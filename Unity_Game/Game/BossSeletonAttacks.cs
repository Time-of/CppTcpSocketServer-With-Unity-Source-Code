using System.Collections;
using System.Collections.Generic;
using UnityEngine;


// 옛날 코드 주워다 짜맞추기...
// 남은 시간이 많지 않아서... 옛날에 작성했던 코드를 복사하여
// 적당히 돌아가게끔만 붙여넣은 누더기 골렘 같은 코드...
public class BossSeletonAttacks : MonoBehaviour
{
	[SerializeField] Transform[] fireballPos;
	private BossSkeleton parent;
	private float range;
	private float damage = 15.0f;
	private int layer;
	public Projectile fileballPrefab;

	void Start()
	{
		parent = GetComponentInParent<BossSkeleton>();

		range = parent.aggroDistance;
		layer = 1 << LayerMask.NameToLayer("Friendly");
	}

	public void SectorAttack(int angle)
	{
		float angleToCos = Mathf.Cos(angle * Mathf.Deg2Rad);

		foreach (Collider i in Physics.OverlapSphere(parent.transform.position, range, layer))
		{
			Vector3 targetNormal = new Vector3(i.transform.position.x - parent.transform.position.x, 0f, i.transform.position.z - parent.transform.position.z).normalized;
			if (Vector3.Dot(parent.transform.forward, targetNormal) >= angleToCos)
			{
				Actor target = i.GetComponent<Actor>();

				if (target != null)
				{
					target.TakeDamage(damage);
				}
			}
		}

		///additionalDamage = 0;
		//crashFX = false;
	}


	public void CreateFireBalls(int num)
	{
		Vector3 targetPos;

		if (parent.targetActor != null)
		{
			targetPos = parent.targetActor.transform.position + Vector3.up * 0.6f;
			Projectile fireball = Instantiate(fileballPrefab);

			if (fireballPos[num] != null)
				fireball.transform.position = fireballPos[num].position;
			else
				fireball.transform.position = transform.position;

			fireball.transform.LookAt(targetPos);
			fireball.Init(parent.targetActor);
		}
	}
}

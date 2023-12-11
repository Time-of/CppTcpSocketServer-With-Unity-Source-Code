using CVSP;
using System.Collections;
using System.Collections.Generic;
using Unity.VisualScripting;
using UnityEngine;



/// <summary>
/// 캐릭터 클래스
/// </summary>
[RequireComponent(typeof(CharacterController))]
public class Character : Actor
{
	[Space(2)]
	[Header("캐릭터")]

	[SerializeField] private CharacterController movement;
	private Camera cam = null;
	[SerializeField] private CharacterAnimator animComp;

	[SerializeField] private float moveSpeed = 3.0f;


	// 전투 기능들
	private const float defaultAttackDelay = 1.0f;
	[SerializeField] private float currentAttackDelay = defaultAttackDelay;
	[SerializeField] private float health = 100.0f;
	private WaitForSeconds battleToPreDelay = new(5.0f);
	[SerializeField] private bool bIsBattleState = false;
	[SerializeField] private bool bIsDead = false;
	[SerializeField] private Transform firePos;
	[SerializeField] private Projectile bulletPrefab;
	public Actor attackTarget;
	public GameObject shotVfxPrefab;


	private void Start()
	{
		bCanBeDamaged = true;
	}


	protected override void OnInitialized()
	{
		if (!bIsMine) return;

		PlayerClient.instance.myController.Possess(this);
		cam = Camera.main;
	}


	public override void NetUpdate(float receivedTime)
	{
		float deltaReceived = Mathf.Abs(receivedTime - lastReceivedTime);
		netPosition += netVelocity * deltaReceived;
		netVelocity = netPosition - transform.position;
		movement.SimpleMove(netVelocity * (1 / sendSerializeDelay));
		//animComp.velocity = FLMathHelper.NearlyEqual(netVelocity, Vector3.zero, 0.1f) ? Vector3.zero : Vector3.one * 3;
		transform.rotation = Quaternion.Slerp(transform.rotation, netRotation, Time.fixedDeltaTime * 10.0f);
		lastReceivedTime = receivedTime;
	}




	private void Update()
	{
		if (!bIsMine || bIsDead) return;
		
		currentAttackDelay -= Time.deltaTime;
		if (currentAttackDelay < 0.0f)
		{
			SendAttackRpc();

			currentAttackDelay += defaultAttackDelay;
		}

		if (!bIsBattleState)
		{
			animComp.animator.SetFloat("Battle", 0.0f, Time.deltaTime * 5.0f, Time.deltaTime);
		}

		cam.transform.position = Vector3.Slerp(cam.transform.position, transform.position + new Vector3(0.0f, 4.5f, -2.5f), Time.deltaTime * moveSpeed);
	}


	public void MoveForward(float value)
	{
		if (bIsDead) return;

		movement.SimpleMove(Vector3.forward * value * moveSpeed);
		float absValue = Mathf.Abs(value);
		if (absValue <= 0.01f) return;

		currentAttackDelay = defaultAttackDelay;
	}


	public void MoveRight(float value)
	{
		if (bIsDead) return;

		movement.SimpleMove(Vector3.right * value * moveSpeed);
		float absValue = Mathf.Abs(value);
		if (absValue <= 0.01f) return;

		currentAttackDelay = defaultAttackDelay;
	}



	private void SendAttackRpc()
	{
		if (!bIsMine) return;	

		RpcInfo info = new()
		{
			actorGuid = guid,
			functionName = "TryAttack"
		};
		CVSPv2Serializer serializer = new();
		serializer.Initialize();
		serializer.Push(info);
		serializer.QuickSendSerializedAndInitialize(CVSPv2.Command.Rpc, RPCTarget.AllClients, true);
	}


	public float tryAttackRadius = 20.0f;
	public void TryAttack()
	{
		var cols = Physics.OverlapSphere(transform.position, tryAttackRadius, 1 << LayerMask.NameToLayer("Enemy"), QueryTriggerInteraction.Ignore);
		
		foreach (var col in cols)
		{
			Actor actor = col.GetComponent<Actor>();
			if (actor == null) continue;
			attackTarget = actor;
			break;
		}

		if (attackTarget == null) return;

		netRotation = Quaternion.Euler(0.0f, Quaternion.LookRotation(attackTarget.transform.position - transform.position, Vector3.up).eulerAngles.y, 0.0f);
		transform.rotation = netRotation;

		animComp.animator.SetTrigger("Attack");
		StopCoroutine("BattleToPreCoroutine");
		StartCoroutine(BattleToPreCoroutine());
	}


	private void OnDrawGizmos()
	{
		Gizmos.color = Color.yellow;
		Gizmos.DrawWireSphere(transform.position, tryAttackRadius);
	}


	IEnumerator BattleToPreCoroutine()
	{
		bIsBattleState = true;
		animComp.animator.SetFloat("Battle", 1.0f);
		yield return battleToPreDelay;
		bIsBattleState = false;
	}


	public override void OnDamaged(float damage)
	{
		if (bIsDead) return;

		health -= damage;

		if (attackTarget != null)
			movement.SimpleMove((transform.position - attackTarget.transform.position).normalized * 30.0f);

		if (health <= 0.0f)
		{
			bCanBeDamaged = false;
			bIsDead = true;
			health = 0.0f;


			animComp.animator.SetLayerWeight(animComp.animator.GetLayerIndex("Upperbody"), 0.0f);
			animComp.animator.SetBool("Dead", true);
			animComp.animator.SetTrigger("Die");
		}
	}


	public void AnimEvent_PerformAttack()
	{
		if (bIsDead || attackTarget == null) return;

		var shotVfx = Instantiate(shotVfxPrefab, firePos.position, firePos.rotation);
		Destroy(shotVfx.gameObject, 3.0f);

		var bullet = Instantiate(bulletPrefab, firePos.position, firePos.rotation);
		bullet.Init(attackTarget);
	}
}

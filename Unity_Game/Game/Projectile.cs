using System.Collections;
using System.Collections.Generic;
using UnityEngine;


/// <summary>
/// 투사체 액터 클래스.
/// </summary>
public class Projectile : MonoBehaviour
{
	public float damage = 10.0f;
	public float speed = 1.5f;
	public Actor target;
	bool bInitialized = false;


	private void Start()
	{
		Invoke("CheckValid", 1.0f);
	}


	private void CheckValid()
	{
		if (target == null) Destroy(gameObject);
		bInitialized = true;
	}


	public void Init(Actor targetActor)
	{
		target = targetActor;

		if (target == null)
			Destroy(gameObject);
		else bInitialized = true;
	}


	private void Update()
	{
		if (!bInitialized) return;

		if (target == null)
		{
			Destroy(gameObject);
			return;
		}

		transform.rotation = Quaternion.Slerp(transform.rotation,
			Quaternion.LookRotation((target.transform.position + Vector3.up - transform.position) * 10.0f, Vector3.up),
			Time.deltaTime * speed);
		
		Vector3 velocity = transform.forward * speed * Time.deltaTime;

		transform.position += velocity;

		if (Physics.Linecast(transform.position, transform.position + velocity, out RaycastHit hitinfo))
		{
			Debug.Log(hitinfo.collider.gameObject);

			Actor actor = hitinfo.collider.gameObject.GetComponent<Actor>();
			if (actor != null && actor == target)
			{
				target.TakeDamage(damage);
			}


			Destroy(gameObject);
		}
	}
}

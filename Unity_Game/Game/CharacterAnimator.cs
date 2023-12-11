using System.Collections;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using Unity.VisualScripting;
using UnityEngine;


public class CharacterAnimator : MonoBehaviour
{
	[SerializeField] private CharacterController movement;
	public Animator animator;
	[SerializeField] private Character character;
	//public Vector3 velocity = Vector3.zero;
	Vector3 velocity = Vector3.zero;
	Vector3 lastPosition = Vector3.zero;

	private void FixedUpdate()
    {
		if (!character.bIsMine) return;
		velocity = FLMathHelper.GetVelocity(lastPosition, transform.position, Time.deltaTime);
		UpdateAnimation(velocity);
		lastPosition = transform.position;
	}


	


	private void OnAnimatorMove()
	{
		movement.SimpleMove(animator.deltaPosition);
	}


	private void UpdateAnimation(Vector3 velocity)
	{
		animator.SetFloat("Speed", character.bIsMine ? velocity.sqrMagnitude : (character.netVelocity.sqrMagnitude > 0.0f ? 3.0f : 0.0f), Time.deltaTime, Time.deltaTime);

		if (velocity != Vector3.zero)
			transform.rotation = Quaternion.Slerp(transform.rotation,
			Quaternion.Euler(0.0f, Quaternion.LookRotation(velocity * 10.0f, Vector3.up).eulerAngles.y, 0.0f),
			Time.deltaTime * 10.0f);
	}
}

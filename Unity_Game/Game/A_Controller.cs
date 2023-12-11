using CVSP;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;


/// <summary>
/// 플레이어 입력을 받아 캐릭터 조작을 담당할 액터.
/// PlayerClient에 의해 생성 요청이 전송된다.
/// 해당 클래스가 생성되었을 때, 캐릭터 Actor 생성 요청을 전송한다.
/// </summary>
public class A_Controller : Actor
{
    protected override void OnInitialized()
	{
		if (!bIsMine) return;

		PlayerClient.instance.myController = this;

		// 캐릭터 스폰 요청
		ActorSpawnParams characterSpawnParams = new()
		{
			actorName = "Character",
			playerId = ownerId,
			transform = new()
		};
		characterSpawnParams.transform.position = new Vector3((ownerId) % 4 - 2, 0, 0).GetUnityVector3();

		CVSPv2Serializer serializer = new();
		serializer.Push(characterSpawnParams);
		serializer.QuickSendSerializedAndInitialize(CVSPv2.Command.SpawnActor, CVSPv2.Option.Success, true);
 	}


	private Character controlledCharacter = null;


	public void Possess(Character newCharacter)
	{
		controlledCharacter = newCharacter;
	}


	private void Update()
	{
		if (!bIsMine || controlledCharacter == null) return;

		float vertical = Input.GetAxis("Vertical");
		float horizontal = Input.GetAxis("Horizontal");

		controlledCharacter.MoveForward(vertical);
		controlledCharacter.MoveRight(horizontal);
	}
}

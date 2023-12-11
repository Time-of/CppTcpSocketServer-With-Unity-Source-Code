using CVSP;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;


/// <summary>
/// �÷��̾� �Է��� �޾� ĳ���� ������ ����� ����.
/// PlayerClient�� ���� ���� ��û�� ���۵ȴ�.
/// �ش� Ŭ������ �����Ǿ��� ��, ĳ���� Actor ���� ��û�� �����Ѵ�.
/// </summary>
public class A_Controller : Actor
{
    protected override void OnInitialized()
	{
		if (!bIsMine) return;

		PlayerClient.instance.myController = this;

		// ĳ���� ���� ��û
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

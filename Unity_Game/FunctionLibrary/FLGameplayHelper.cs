using CVSP;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;


/// <summary>
/// �����÷��̸� ��Ȱ�ϰ� �� �� �ֵ��� �����ִ� �Լ� ���̺귯�� Ŭ����
/// </summary>
public class FLGameplayHelper : MonoBehaviour
{
	public static Dictionary<uint, Actor> actorDictionary = new(64);


    public static void SpawnActor(ActorSpawnParams spawnParams)
    {
		Debug.Log("SpawnActor �̸�: " + spawnParams.actorName + ", ���� id: " + spawnParams.playerId);

		Actor loadedActor = Resources.Load<Actor>(spawnParams.actorName);
		if (loadedActor == null)
		{
			Debug.LogError("SpawnActor �� ���� " + spawnParams.actorName + "�� Resources���� �ҷ����� ����!");
			return;
		}

		Actor spawnedActor = Instantiate(loadedActor, spawnParams.transform.position.Get(), spawnParams.transform.rotation.Get());
		if (spawnedActor == null)
		{
			Debug.LogError("SpawnActor �� ���� " + spawnParams.actorName + " ������ ������!");
			return;
		}

		actorDictionary[spawnParams.transform.actorGuid] = spawnedActor;
		spawnedActor.name = "[" + spawnParams.playerId + "] " + spawnParams.actorName;
		spawnedActor.Initialize(spawnParams.transform.actorGuid, spawnParams.playerId);
	}


	public static void SpawnActorNetObserver(TransformInfo transformInfo)
	{
		uint actorGuid = transformInfo.actorGuid;

		Actor foundActor = null;
		if (!actorDictionary.TryGetValue(actorGuid, out foundActor))
		{
			Debug.LogWarning("<color=orange>���� [" + actorGuid + "]�� ���� ������ ���� ����! ���Ͱ� actorDictionary�� ����!</color>");
			return;
		}

		ActorNetObserver loadedObserver = Resources.Load<ActorNetObserver>("ActorNetObserver");
		if (loadedObserver == null)
		{
			Debug.LogError("<color=red>ActorNetObserver�� Resources���� �ҷ����� ����!</color>");
			return;
		}

		ActorNetObserver spawnedObserver = Instantiate(loadedObserver);
		if (spawnedObserver == null)
		{
			Debug.LogError("<color=red>ActorNetObserver ������ ������!</color>");
			return;
		}

		Debug.Log("SpawnActorNetObserver: ���� [" + actorGuid + "]�� ������ ����!");
		PlayerClient.actorObservers[actorGuid] = spawnedObserver;

		Debug.Log("SpawnActorNetObserver: ������ ������ " + PlayerClient.actorObservers[actorGuid].name + "!");
		spawnedObserver.Initialize(foundActor, actorGuid, foundActor.ownerId, transformInfo.position.Get(), transformInfo.rotation.Get());
	}
}

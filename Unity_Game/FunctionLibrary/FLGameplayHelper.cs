using CVSP;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;


/// <summary>
/// 게임플레이를 원활하게 할 수 있도록 도와주는 함수 라이브러리 클래스
/// </summary>
public class FLGameplayHelper : MonoBehaviour
{
	public static Dictionary<uint, Actor> actorDictionary = new(64);


    public static void SpawnActor(ActorSpawnParams spawnParams)
    {
		Debug.Log("SpawnActor 이름: " + spawnParams.actorName + ", 오너 id: " + spawnParams.playerId);

		Actor loadedActor = Resources.Load<Actor>(spawnParams.actorName);
		if (loadedActor == null)
		{
			Debug.LogError("SpawnActor 중 액터 " + spawnParams.actorName + "를 Resources에서 불러오지 못함!");
			return;
		}

		Actor spawnedActor = Instantiate(loadedActor, spawnParams.transform.position.Get(), spawnParams.transform.rotation.Get());
		if (spawnedActor == null)
		{
			Debug.LogError("SpawnActor 중 액터 " + spawnParams.actorName + " 생성에 실패함!");
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
			Debug.LogWarning("<color=orange>액터 [" + actorGuid + "]에 대한 옵저버 생성 실패! 액터가 actorDictionary에 없음!</color>");
			return;
		}

		ActorNetObserver loadedObserver = Resources.Load<ActorNetObserver>("ActorNetObserver");
		if (loadedObserver == null)
		{
			Debug.LogError("<color=red>ActorNetObserver를 Resources에서 불러오지 못함!</color>");
			return;
		}

		ActorNetObserver spawnedObserver = Instantiate(loadedObserver);
		if (spawnedObserver == null)
		{
			Debug.LogError("<color=red>ActorNetObserver 생성에 실패함!</color>");
			return;
		}

		Debug.Log("SpawnActorNetObserver: 액터 [" + actorGuid + "]의 옵저버 생성!");
		PlayerClient.actorObservers[actorGuid] = spawnedObserver;

		Debug.Log("SpawnActorNetObserver: 생성된 옵저버 " + PlayerClient.actorObservers[actorGuid].name + "!");
		spawnedObserver.Initialize(foundActor, actorGuid, foundActor.ownerId, transformInfo.position.Get(), transformInfo.rotation.Get());
	}
}

using CVSP;
using System.Collections;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Threading;
using UnityEngine;


/// <summary>
/// ** 사용되지 않음 **
/// 액터 동기화 작업을 담당해 줄 스레드를 돌리는 클래스. (였던 것)
/// ThreadWorksDistributor가 게임 씬에서 생성함.
/// 
/// 주 작업은 역직렬화하여 PlayerClient의 액터 옵저버에게 전달하는 것...
/// 이었지만, 내가 모르는 오류로 인해 정상적으로 작동하지 않는 것 같아 폐기된 클래스.  
/// 시간이 더 있었다면 분명히 사용했을 것 같아서 아쉽다.
/// </summary>
public class SyncActorWorker : MonoBehaviour
{
	public static ConcurrentQueue<SyncActorWorkerData> dataQueue = new();
	public static bool bNeedToWork { get; set; }


	private TransformInfo transformInfo = new();
	private ActorPositionInfo positionInfo = new();
	private ActorRotationInfo rotationInfo = new();


	private void Start()
    {
		dataQueue.Clear();
		bNeedToWork = true;

		Thread workerThread = new Thread(WorkerThread);
		workerThread.Start();
    }

	
	public void WorkerThread()
	{
		Debug.Log("<b>WorkerThread 시작! bNeedToWork: </b>" + bNeedToWork);

		while (bNeedToWork)
		{
			if (!dataQueue.TryDequeue(out SyncActorWorkerData data)) continue;

			switch (data.option)
			{
				
				case CVSPv2.Option.Success:
					{
						transformInfo.Deserialize(data.actorSyncSerializer);
						if (PlayerClient.actorObservers.TryGetValue(transformInfo.actorGuid, out ActorNetObserver observer))
						{
							observer.netPosition = transformInfo.position.Get();
							observer.netRotation = transformInfo.rotation.Get();
							Debug.Log("워커 스레드 트랜스폼: " + transformInfo.actorGuid + ", pos: " + observer.netPosition + ", rot: " + observer.netRotation);
						}
					}
					break;

				case CVSPv2.Option.SyncActorPositionOnly:
					{
						positionInfo.Deserialize(data.actorSyncSerializer);
						if (PlayerClient.actorObservers.TryGetValue(positionInfo.actorGuid, out ActorNetObserver observer))
						{
							observer.netPosition = positionInfo.position.Get();
							Debug.Log("워커 스레드 포지션: " + transformInfo.actorGuid + ", pos: " + observer.netPosition);
						}
					}
					break;

				case CVSPv2.Option.SyncActorRotationOnly:
					{
						rotationInfo.Deserialize(data.actorSyncSerializer);
						if (PlayerClient.actorObservers.TryGetValue(rotationInfo.actorGuid, out ActorNetObserver observer))
						{
							observer.netRotation = rotationInfo.rotation.Get();
							Debug.Log("워커 스레드 로테이션: " + transformInfo.actorGuid + ", rot: " + observer.netRotation);
						}
					}
					break;

				default:
					Debug.LogWarning("SyncActorWorker: 이상한 option이 들어옴!: " + data.option);
					break;

			}
		}

		dataQueue.Clear();

		Debug.Log("<b>WorkerThread 종료! bNeedToWork: </b>" + bNeedToWork);
	}
}

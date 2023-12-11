using CVSP;
using System.Collections;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Threading;
using UnityEngine;


/// <summary>
/// ** ������ ���� **
/// ���� ����ȭ �۾��� ����� �� �����带 ������ Ŭ����. (���� ��)
/// ThreadWorksDistributor�� ���� ������ ������.
/// 
/// �� �۾��� ������ȭ�Ͽ� PlayerClient�� ���� ���������� �����ϴ� ��...
/// �̾�����, ���� �𸣴� ������ ���� ���������� �۵����� �ʴ� �� ���� ���� Ŭ����.  
/// �ð��� �� �־��ٸ� �и��� ������� �� ���Ƽ� �ƽ���.
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
		Debug.Log("<b>WorkerThread ����! bNeedToWork: </b>" + bNeedToWork);

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
							Debug.Log("��Ŀ ������ Ʈ������: " + transformInfo.actorGuid + ", pos: " + observer.netPosition + ", rot: " + observer.netRotation);
						}
					}
					break;

				case CVSPv2.Option.SyncActorPositionOnly:
					{
						positionInfo.Deserialize(data.actorSyncSerializer);
						if (PlayerClient.actorObservers.TryGetValue(positionInfo.actorGuid, out ActorNetObserver observer))
						{
							observer.netPosition = positionInfo.position.Get();
							Debug.Log("��Ŀ ������ ������: " + transformInfo.actorGuid + ", pos: " + observer.netPosition);
						}
					}
					break;

				case CVSPv2.Option.SyncActorRotationOnly:
					{
						rotationInfo.Deserialize(data.actorSyncSerializer);
						if (PlayerClient.actorObservers.TryGetValue(rotationInfo.actorGuid, out ActorNetObserver observer))
						{
							observer.netRotation = rotationInfo.rotation.Get();
							Debug.Log("��Ŀ ������ �����̼�: " + transformInfo.actorGuid + ", rot: " + observer.netRotation);
						}
					}
					break;

				default:
					Debug.LogWarning("SyncActorWorker: �̻��� option�� ����!: " + data.option);
					break;

			}
		}

		dataQueue.Clear();

		Debug.Log("<b>WorkerThread ����! bNeedToWork: </b>" + bNeedToWork);
	}
}

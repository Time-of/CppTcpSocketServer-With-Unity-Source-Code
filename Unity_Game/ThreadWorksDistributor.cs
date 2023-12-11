using CVSP;
using System;
using System.Collections;
using System.Collections.Concurrent;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Events;
using UnityEngine.SceneManagement;


public class ThreadWorksDistributor : MonoBehaviour
{
	public static ThreadWorksDistributor instance;

	public ConcurrentQueue<UnityAction> actionsQueue = new();

	public UnityAction OnEnterButtonPressedDelegate;

	[SerializeField] private SyncActorWorker workerPrefab;
	private SyncActorWorker worker = null;


	public void Awake()
	{
		if (instance == null)
		{
			instance = this;
		}
		else if (instance != this)
		{
			Destroy(gameObject);
		}

		SceneManager.sceneLoaded += OnLevelLoaded;
		DontDestroyOnLoad(gameObject);
	}


	private void OnLevelLoaded(Scene arg0, LoadSceneMode arg1)
	{
		actionsQueue.Clear();
		OnEnterButtonPressedDelegate = null;
		Debug.Log("ThreadWorksDistributor: �׼� ť�� ���� ���� Delegate �ʱ�ȭ��!");

		if (PlayerClient.instance.bIsInGame)
		{
			Debug.Log("ThreadWorksDistributor: SyncActorWorker ����!");
			worker = Instantiate(workerPrefab);
		}
	}


	public void Update()
	{
		while (actionsQueue.TryDequeue(out UnityAction action))
		{
			action.Invoke();
		}

		if (Input.GetKeyDown(KeyCode.Return) && OnEnterButtonPressedDelegate != null)
		{
			OnEnterButtonPressedDelegate.Invoke();
		}
	}
}

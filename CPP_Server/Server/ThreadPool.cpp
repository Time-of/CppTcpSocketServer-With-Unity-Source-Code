#include "pch.h"

#include "ThreadPool.h"


ThreadPool::ThreadPool(uint8 threadCount) noexcept
{
	for (uint8 i = 0; i < threadCount; ++i)
		workerThreadArray.emplace_back([this]()-> void
		{
			while (true)
			{
				std::function<void()> func;
				{
					std::unique_lock<std::mutex> executeTaskLock(queueMutex);
					notifyWorkCondition.wait(executeTaskLock, [this]()-> bool { return bIsPoolDead or !taskQueue.empty(); });

					// 남은 작업이 완전히 없는 경우에만 종료
					if (bIsPoolDead and taskQueue.empty()) return;

					func = std::move(taskQueue.front());
					taskQueue.pop();
				}

				// 여기서 실행
				func();
			}
		});

	LOG_TRACE("스레드 풀 생성 완료");
}


ThreadPool::~ThreadPool()
{
	{
		std::unique_lock<std::mutex> taskQueueLock(queueMutex);
		bIsPoolDead = true;
	}

	notifyWorkCondition.notify_all();

	for (auto& worker : workerThreadArray)
	{
		worker.join();
	}
}

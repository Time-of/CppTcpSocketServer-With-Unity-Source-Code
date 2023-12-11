#pragma once

#include <mutex>
#include <queue>
#include <future>
#include <vector>
#include <condition_variable>
#include <type_traits>


class ThreadPool
{
public:
	explicit ThreadPool() noexcept = delete;
	explicit ThreadPool(uint8 threadCount) noexcept;
	~ThreadPool();


public:
	template<class Func, class... Args>
	void AddTask(Func&& func, Args&&... args);


private:
	bool bIsPoolDead = false;

	std::mutex queueMutex;
	std::queue<std::function<void()>> taskQueue;
	std::vector<std::thread> workerThreadArray;
	std::condition_variable notifyWorkCondition;


private:
	explicit ThreadPool(const ThreadPool& other) = delete;
	explicit ThreadPool(ThreadPool&& other) noexcept = delete;
	ThreadPool& operator=(const ThreadPool& other) = delete;
	ThreadPool& operator=(ThreadPool&& other) noexcept = delete;
};


template<class Func, class ...Args>
inline void ThreadPool::AddTask(Func&& func, Args && ...args)
{
	auto functonTask = std::make_shared<std::packaged_task<void()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
	
	{
		std::unique_lock<std::mutex> lock(queueMutex);
		assert(!bIsPoolDead and "스레드 풀이 이미 소멸되었는데 AddTask를 호출하려 합니다!");

		taskQueue.push([functonTask]()-> void { (*functonTask)(); });
	}

	notifyWorkCondition.notify_one();
}

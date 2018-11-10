#pragma once

#include <mutex>

class StdMutexLocker{
public:
	StdMutexLocker(std::mutex& mutex) : m_Mutex(mutex)	{
		m_Mutex.lock();
	}

	~StdMutexLocker(){
		m_Mutex.unlock();
	}
private:
	std::mutex& m_Mutex;
};

class StdRecursiveMutexLocker {
public:
	StdRecursiveMutexLocker(std::recursive_mutex& mutex) : myMutex(mutex) {
		myMutex.lock();
	}

	~StdRecursiveMutexLocker() {
		myMutex.unlock();
	}
private:
	std::recursive_mutex& myMutex;
};

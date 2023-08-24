#pragma once
#include <mutex>
#include <memory>

template<typename T>
class SharedObjectManager
{
	struct sharedMemory
	{
		std::shared_ptr<T> mem;
		float sampleRate = {}; // -1 = any sample-rate.
		int id = {};
	};

	inline static std::vector<sharedMemory> storage;
	inline static std::mutex mtx;

public:
	static std::shared_ptr<T> getOrCreateSharedMemory(
		float sampleRate,
		int id,
		std::function<std::shared_ptr<T>(float sampleRate)> initMemory
		)
	{
		std::scoped_lock l{mtx};

		for(auto& item : storage)
		{
			if(item.sampleRate == sampleRate && item.id == id)
			{
				return item.mem;
			}
		}

		storage.push_back({initMemory(sampleRate), sampleRate, id});

		return storage.back().mem;
	}
};

#pragma once
#include <functional>
#include <thread>
#include "../shared/xplatform.h"

#if defined(_WIN32)
#include <Windows.h>
typedef HANDLE nativeHandle;
#else
typedef int nativeHandle;
#endif

/*
#include "../shared/FileWatcher.h"
*/

namespace file_watcher
{
	class FileWatcher
	{
		std::thread backgroundThread;
#if defined(_WIN32)
		nativeHandle stopEvent = {};
#endif
#if defined(__APPLE__)
        std::function<void()> callback;
#endif
	public:
		void Start(const platform_string & fullPath, std::function<void()> callback);
		~FileWatcher();
	};
}


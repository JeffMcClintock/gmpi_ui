#include "FileWatcher.h"
#include "../shared/string_utilities.h"
#include "../shared/it_enum_list.h"
#include "../shared/string_utilities.h"
#include "../se_sdk3/MpString.h"
#include <thread>

#if defined(__APPLE__)
#include "CoreFoundation/CoreFoundation.h"
#endif

using namespace std;
using namespace gmpi;
using namespace gmpi_sdk;

namespace file_watcher
{
#if defined(_WIN32)

	void WatchDirectoryPrivate(HANDLE stopEvent, platform_string fullPath, std::function<void()> callback)
	{
		HANDLE eventHandles[2];
		TCHAR lpDrive[4];
		TCHAR lpFile[_MAX_FNAME];
		TCHAR lpExt[_MAX_EXT];

		eventHandles[0] = stopEvent;

		_tsplitpath_s(fullPath.c_str(), lpDrive, 4, NULL, 0, lpFile, _MAX_FNAME, lpExt, _MAX_EXT);

		lpDrive[2] = (TCHAR)'\\';
		lpDrive[3] = (TCHAR)'\0';

		// Watch the directory for file creation and deletion. 
		eventHandles[1] = FindFirstChangeNotification(
			fullPath.c_str(),		       // directory to watch 
			FALSE,                         // do not watch subtree 
			FILE_NOTIFY_CHANGE_FILE_NAME); // watch file name changes (plus creation/deletion)

		if(eventHandles[0] == INVALID_HANDLE_VALUE || eventHandles[1] == INVALID_HANDLE_VALUE)
		{
			printf("\n ERROR: FindFirstChangeNotification function failed.\n");
			// ExitProcess(GetLastError());
			return;
		}

		// Make a final validation check on our handles.
		if(eventHandles[0] == NULL || eventHandles[1] == NULL)
		{
			printf("\n ERROR: Unexpected NULL from FindFirstChangeNotification.\n");
			return;
		}

		// Change notification is set. Now wait on both notification 
		// handles and refresh accordingly. 

		while(TRUE)
		{
			// Wait for notification.
			const auto dwWaitStatus = WaitForMultipleObjects(2, eventHandles, FALSE, INFINITE);

			switch(dwWaitStatus)
			{
			case WAIT_OBJECT_0:
				//_RPT0(_CRT_WARN, "FileWatcher: terminate signalled.\n");
				return; // we are done.
				break;

			case WAIT_OBJECT_0 + 1:

				// A file was created, renamed, or deleted in the directory.
				// Refresh this directory and restart the notification.
				//_RPT0(_CRT_WARN, "FileWatcher: file change signalled.\n");

				callback();
				if(FindNextChangeNotification(eventHandles[1]) == FALSE)
				{
					printf("\n ERROR: FindNextChangeNotification function failed.\n");
					return;
				}
				break;

			case WAIT_TIMEOUT:

				// A timeout occurred, this would happen if some value other 
				// than INFINITE is used in the Wait call and no changes occur.
				// In a single-threaded environment you might not want an
				// INFINITE wait.

				printf("\nNo changes in the timeout period.\n");
				break;

			default:
				printf("\n ERROR: Unhandled dwWaitStatus.\n");
				return;
				break;
			}
		}
	}
#endif

#if defined(__APPLE__) && !defined(TARGET_OS_IPHONE)
void fse_handle_events(
  ConstFSEventStreamRef stream,
  void *data,
  size_t numEvents,
  void *eventPaths,
  const FSEventStreamEventFlags eventFlags[],
  const FSEventStreamEventId eventIds[]
)
{
    auto& callback = (std::function<void()>&) data;
    callback();
    /* TODO
  fse_watcher_t watcher = data;
  if (!watcher->handler) return;
  fse_event_t *events = malloc(sizeof(*events) * numEvents);
  CHECK(events);
  size_t idx;
  for (idx=0; idx < numEvents; idx++) {
    fse_event_t *event = &events[idx];
    CFStringRef path = (CFStringRef)CFArrayGetValueAtIndex((CFArrayRef)eventPaths, idx);
    if (!CFStringGetCString(path, event->path, PATH_MAX, kCFStringEncodingUTF8)) {
      event->path[0] = 0;
    }
    event->id = eventIds[idx];
    event->flags = eventFlags[idx];
  }
  if (!watcher->handler) {
    free(events);
  } else {
    watcher->handler(watcher->context, numEvents, events);
  }
     */
}

    // https://github.com/fsevents/fsevents/blob/master/src/rawfsevents.c
    void WatchDirectoryPrivate(int stopEvent_todo, platform_string fullPath, std::function<void()>& callback)
    {
//        strncpy(watcher->path, path, PATH_MAX);
//        watcher->handler = handler;
//        watcher->context = context;
        auto loop = CFRunLoopGetCurrent();
        CFRunLoopPerformBlock(loop, kCFRunLoopDefaultMode, ^(){
                
            //          if (hookstart)
            //              hookstart(watcher->context);
            void* info = (void*) &callback;
            FSEventStreamContext streamcontext = { 0, info, NULL, NULL, NULL };
                
            CFStringRef dirs[] = { CFStringCreateWithCString(NULL, fullPath.c_str(), kCFStringEncodingUTF8) };
                
            auto stream = FSEventStreamCreate(
                    NULL,
                    &fse_handle_events,
                    &streamcontext,
                    CFArrayCreate(NULL, (const void **)&dirs, 1, NULL),
                    kFSEventStreamEventIdSinceNow,
                    (CFAbsoluteTime) 0.1,
                    kFSEventStreamCreateFlagNone | kFSEventStreamCreateFlagWatchRoot | kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagUseCFTypes
                );
                
              FSEventStreamScheduleWithRunLoop(stream, loop, kCFRunLoopDefaultMode);
                
              FSEventStreamStart(stream);
            }
        );
        CFRunLoopWakeUp(loop);
    }
#endif

	void FileWatcher::Start(const platform_string& fullPath, std::function<void()> pCallback)
	{
#if defined(_WIN32)
		//_RPT1(_CRT_WARN, "FileWatcher::Start(%S)\n", fullPath.c_str());
		stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		backgroundThread = std::thread(
            [stopEvent=stopEvent, fullPath, pCallback]
            {
                WatchDirectoryPrivate(stopEvent, fullPath, pCallback);
            }
        );
#endif
#if defined(__APPLE__)
        callback = pCallback;
/* TODO
        backgroundThread = std::thread(
            [fullPath, this]
            {
                WatchDirectoryPrivate(0, fullPath, callback);
            }
        );
 */
#endif
	}

	FileWatcher::~FileWatcher()
	{
#if defined(_WIN32)
//		_RPT0(_CRT_WARN, "~FileWatcher()\n");
		SetEvent(stopEvent);    // ask thread to stop

		if (backgroundThread.joinable())
		{
			backgroundThread.join();
		}
#endif
	}

}

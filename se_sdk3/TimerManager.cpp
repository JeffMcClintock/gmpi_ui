
#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include "Windows.h"

#else

#import <CoreFoundation/CoreFoundation.h>

#endif

#include <algorithm>
#include "TimerManager.h"
#include "assert.h"

// time (in ms) between VST onidle calls (via WM_TIMER)
#define IDLE_PERIOD 50

#ifdef _WIN32

void CALLBACK SynthEditVstTimerProc(
   HWND /*hWnd*/,      // handle of CWnd that called SetTimer
   UINT /*nMsg*/,      // WM_TIMER
   UINT_PTR nIDEvent,   // timer identification
   DWORD /*dwTime*/    // system time
)
{
	static bool reentrant = false;

	if (!reentrant)
	{
		reentrant = true;
		TimerManager::Instance()->OnTimer(nIDEvent);
		reentrant = false;
	}
}

#else
void timerCallback(CFRunLoopTimerRef t, void *info)
{
    /* info was null!!
     
	TimerManager* timer = (TimerManager*)info;
    const se_sdk_timers::timer_id_t timerIdTodo = 0; // TODO
	timer->OnTimer(timerIdTodo);
    */
    //void* nIDEvent = info;
    //TimerManager::Instance()->OnTimer(nIDEvent);
    
    auto timer = (se_sdk_timers::Timer*)info;

    TimerManager::Instance()->OnTimer(timer->idleTimer_);
}
#endif

TimerManager* TimerManager::Instance()
{
	static TimerManager timerManager_;
	return &timerManager_;
}

void TimerManager::OnTimer(se_sdk_timers::timer_id_t timerId)
{
	for (auto& timer : timers)
	{
		if(timer.idleTimer_ == timerId)
		{
			timer.OnTimer();
			return;
		}
	}
}


TimerManager::TimerManager() :
interval_( IDLE_PERIOD )
{
}

void TimerManager::SetInterval( int intervalMs )
{
	interval_ = intervalMs;
//	StartTimer();
}

namespace se_sdk_timers
{
    bool Timer::isRunning()
    {
        return idleTimer_ != 0;
    }
	void Timer::Start()
	{
		Stop();

		if (!clients_.empty())
		{
			// Start timer to provide onidle events.

#ifdef _WIN32

			idleTimer_ = SetTimer(
				0,	// HWND. NULL = host main window
				0,	// timer ID. must be 0 if HWND is null
				periodMilliSeconds,
				SynthEditVstTimerProc);

//			_RPT1(_CRT_WARN, "StartTimer %d\n", idleTimer_);

#else
            CFRunLoopTimerContext timerContext = CFRunLoopTimerContext();
			timerContext.info = this;
			idleTimer_ = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + periodMilliSeconds * 0.001f, periodMilliSeconds * 0.001f, 0, 0, timerCallback, &timerContext);
			if (idleTimer_)
				CFRunLoopAddTimer(CFRunLoopGetCurrent(), (CFRunLoopTimerRef)idleTimer_, kCFRunLoopCommonModes);
#endif
		}
	}

	void Timer::Stop()
	{
		if (idleTimer_ != 0)
		{
#ifdef _WIN32

			KillTimer(0, idleTimer_);
//			_RPT1(_CRT_WARN, "KillTimer %d\n", idleTimer_);

#else
			CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), (CFRunLoopTimerRef)idleTimer_, kCFRunLoopCommonModes);
			CFRunLoopTimerInvalidate((CFRunLoopTimerRef)idleTimer_);
			CFRelease((CFRunLoopTimerRef)idleTimer_);
#endif
			idleTimer_ = 0;
		}
	}

void Timer::OnTimer()
{
	clientContainer_t safeCopy(clients_);
	for (auto client : safeCopy)
	{
		// Access VERY cautiously. Anything can happen to clients during callback (add, remove etc).

		// is client still in main list?
		if (std::find(clients_.begin(), clients_.end(), client) == clients_.end())
			continue;

		if (!client->OnTimer())
		{
			clients_.erase(std::remove(clients_.begin(), clients_.end(), client), clients_.end());
		}
	}

	if (clients_.empty())
	{
		Stop();
	}

#if 0 // was flaky - when an element removed, vector shrunk and skipped the next client.

	// Iterate with index, std iterators can become invalidated when new clients added during callback.
	for (int i = 0; i < clients_.size(); ++i)
	{
		auto timerClient = clients_[i];
		if (! timerClient->OnTimer() )
		{
			// Erase very cautiously. Anything could have happened to clients during callback (add, remove etc).
			auto it2 = find(clients_.begin(), clients_.end(), timerClient);
			if (it2 != clients_.end())
			{
				clients_.erase(it2);
			}

			if (clients_.empty())
			{
				Stop();
			}
		}
#endif
	}
}

TimerManager::~TimerManager()
{
	for (auto& timer : timers)
	{
		assert(timer.clients_.empty());
		timer.Stop();
	}
}

void TimerManager::RegisterClient(TimerClient* client, int periodMilliSeconds)
{
	for (auto& timer : timers)
	{
		if (timer.periodMilliSeconds == periodMilliSeconds)
		{
			if (std::find(timer.clients_.begin(), timer.clients_.end(), client) == timer.clients_.end())
			{
				timer.clients_.push_back(client);
				if (!timer.isRunning())
				{
					timer.Start();
				}
			}
			return;
		}
	}

	timers.push_back(periodMilliSeconds);
	timers.back().clients_.push_back(client);
	timers.back().Start();
}

void TimerManager::UnRegisterClient( TimerClient* client )
{
	for (auto it_timers = timers.begin() ; it_timers != timers.end() ; ++it_timers)
	{
		auto& timer = *it_timers;

		auto it = std::find(timer.clients_.begin(), timer.clients_.end(), client);
		if (it != timer.clients_.end())
		{
			timer.clients_.erase(it);

			if (timer.clients_.empty())
			{
				timer.Stop();
				timers.erase(it_timers);
			}
			return;
		}
	}
}

void TimerClient::StartTimer(int periodMilliSeconds)
{
	TimerManager::Instance()->RegisterClient(this, periodMilliSeconds);
}

void TimerClient::SetTimerIntervalMs( int periodMilliSeconds )
{
	TimerManager::Instance()->SetInterval( periodMilliSeconds );
}

void TimerClient::StartTimer()
{
	TimerManager::Instance()->RegisterClient(this);
}

void TimerClient::StopTimer()
{
	TimerManager::Instance()->UnRegisterClient( this );
}

TimerClient::~TimerClient()
{
	StopTimer();
}

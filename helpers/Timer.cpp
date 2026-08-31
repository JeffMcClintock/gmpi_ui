
#ifdef _WIN32
#undef  WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#undef  NOMINMAX
#define NOMINMAX
#include "Windows.h"
#endif

#if __APPLE__
#import <CoreFoundation/CoreFoundation.h>
#endif

#include <algorithm>
#include <chrono>
#include "assert.h"
#include "Timer.h"

// time (in ms) between VST onidle calls (via WM_TIMER)
#define IDLE_PERIOD 50

#ifdef _WIN32

void CALLBACK GMPITimerProc(
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
		gmpi::TimerManager::instance()->onTimer(nIDEvent);
		reentrant = false;
	}
}

#endif

#if __APPLE__
void timerCallback_GMPI_Wrapper(CFRunLoopTimerRef t, void *info)
{
    /* info was null!!
     
	TimerManager* timer = (TimerManager*)info;
    const se_sdk_timers::timer_id_t timerIdTodo = 0; // TODO
	timer->OnTimer(timerIdTodo);
    */
    //void* nIDEvent = info;
    //TimerManager::instance()->OnTimer(nIDEvent);
    
    auto timer = (gmpi::se_sdk_timers::Timer*)info;

    gmpi::TimerManager::instance()->onTimer(timer->idleTimer_);
}
#endif

namespace gmpi
{
	TimerManager* TimerManager::instance()
	{
		static TimerManager timerManager_;
		return &timerManager_;
	}

	void TimerManager::onTimer(se_sdk_timers::timer_id_t timerId)
	{
		for (auto& timer : timers)
		{
			if (timer.idleTimer_ == timerId)
			{
				timer.onTimer();
				return;
			}
		}
	}

	void TimerManager::pump(int elapsedMs)
	{
		for (auto& timer : timers)
		{
			if (!timer.isRunning())
				continue;

			timer.pendingMs += elapsedMs;
			if (timer.pendingMs >= timer.periodMilliSeconds)
			{
				timer.pendingMs = 0;
				timer.onTimer(); // may stop timers / unregister clients; the std::list stays valid.
			}
		}
	}

	void TimerManager::pump()
	{
		using namespace std::chrono;
		const auto nowMs =
			duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();

		// The FIRST call only starts the clock. Measuring from an epoch nobody
		// set would hand the timers however long this process has been alive,
		// firing every client once immediately -- harmless for an idle timer
		// and wrong for anything that reads the elapsed time.
		if (lastPumpMonotonicMs_ == 0)
		{
			lastPumpMonotonicMs_ = nowMs;
			return;
		}

		const auto elapsedMs = nowMs - lastPumpMonotonicMs_;
		if (elapsedMs <= 0)
			return; // a second pumper in the same millisecond; nothing has happened.

		lastPumpMonotonicMs_ = nowMs;
		pump(static_cast<int>(elapsedMs));
	}


	TimerManager::TimerManager() :
		interval_(IDLE_PERIOD)
	{
	}

	void TimerManager::setInterval(int intervalMs)
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
		void Timer::start()
		{
			stop();

			if (!clients_.empty())
			{
				// Start timer to provide onidle events.

#ifdef _WIN32

				idleTimer_ = SetTimer(
					0,	// HWND. NULL = host main window
					0,	// timer ID. must be 0 if HWND is null
					periodMilliSeconds,
					GMPITimerProc);

				//			_RPT1(_CRT_WARN, "StartTimer %d\n", idleTimer_);

#endif
#if __APPLE__
				CFRunLoopTimerContext timerContext = CFRunLoopTimerContext();
				timerContext.info = this;
				idleTimer_ = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + periodMilliSeconds * 0.001f, periodMilliSeconds * 0.001f, 0, 0, timerCallback_GMPI_Wrapper, &timerContext);
				if (idleTimer_)
					CFRunLoopAddTimer(CFRunLoopGetCurrent(), (CFRunLoopTimerRef)idleTimer_, kCFRunLoopCommonModes);
#endif
#if !defined(_WIN32) && !defined(__APPLE__)
				// No native timer source: mark the timer running and rely on the
				// host application driving TimerManager::pump() (see Timer.h).
				idleTimer_ = this;
				pendingMs = 0;
#endif
			}
		}

		void Timer::stop()
		{
			if (idleTimer_ != 0)
			{
#ifdef _WIN32

				KillTimer(0, idleTimer_);
				//			_RPT1(_CRT_WARN, "KillTimer %d\n", idleTimer_);

#endif
#if __APPLE__
				CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), (CFRunLoopTimerRef)idleTimer_, kCFRunLoopCommonModes);
				CFRunLoopTimerInvalidate((CFRunLoopTimerRef)idleTimer_);
				CFRelease((CFRunLoopTimerRef)idleTimer_);
#endif
				idleTimer_ = 0;
			}
		}

		void Timer::onTimer()
		{
			clientContainer_t safeCopy(clients_);
			for (auto client : safeCopy)
			{
				// Access VERY cautiously. Anything can happen to clients during callback (add, remove etc).

				// is client still in main list?
				if (std::find(clients_.begin(), clients_.end(), client) == clients_.end())
					continue;

				if (!client->onTimer())
				{
					clients_.erase(std::remove(clients_.begin(), clients_.end(), client), clients_.end());
				}
			}

			if (clients_.empty())
			{
				stop();
			}

#if 0 // was flaky - when an element removed, vector shrunk and skipped the next client.

			// Iterate with index, std iterators can become invalidated when new clients added during callback.
			for (int i = 0; i < clients_.size(); ++i)
			{
				auto timerClient = clients_[i];
				if (!timerClient->OnTimer())
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
				timer.stop();
			}
		}

		void TimerManager::registerClient(TimerClient* client, int periodMilliSeconds)
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
							timer.start();
						}
					}
					return;
				}
			}

			timers.push_back(periodMilliSeconds);
			timers.back().clients_.push_back(client);
			timers.back().start();
		}

		void TimerManager::unRegisterClient(TimerClient* client)
		{
			for (auto it_timers = timers.begin(); it_timers != timers.end(); ++it_timers)
			{
				auto& timer = *it_timers;

				auto it = std::find(timer.clients_.begin(), timer.clients_.end(), client);
				if (it != timer.clients_.end())
				{
					timer.clients_.erase(it);

					if (timer.clients_.empty())
					{
						timer.stop();
						timers.erase(it_timers);
					}
					return;
				}
			}
		}

		void TimerClient::startTimer(int periodMilliSeconds)
		{
			TimerManager::instance()->registerClient(this, periodMilliSeconds);
		}

		void TimerClient::startTimerHz(int rateHz)
		{
			const auto ms = 1000 / rateHz;
			startTimer(ms);
		}
	
		void TimerClient::setTimerIntervalMs(int periodMilliSeconds)
		{
			TimerManager::instance()->setInterval(periodMilliSeconds);
		}

		void TimerClient::startTimer()
		{
			TimerManager::instance()->registerClient(this);
		}

		void TimerClient::stopTimer()
		{
			TimerManager::instance()->unRegisterClient(this);
		}

		TimerClient::~TimerClient()
		{
			stopTimer();
		}

} // namespace

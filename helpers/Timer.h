#pragma once

#include <vector>
#include <list>

/*
This class provides a programable timer for creating animation effects.
NOTE: all instances of the module share the same timer, and therefore the same timer interval.
*/
namespace gmpi
{

// todo lowercase methods
class TimerClient
{
public:
	virtual ~TimerClient();
	virtual bool onTimer() = 0;

	// New. Better
	void startTimer(int periodMilliSeconds);
	void startTimerHz(int rateHz);
	void stopTimer();

	// old. avoid.
	void setTimerIntervalMs( int periodMilliSeconds );
	void startTimer();
};

typedef std::vector<class TimerClient*> clientContainer_t;

namespace se_sdk_timers
{
#ifdef _WIN32
	typedef unsigned __int64 timer_id_t;
#else
    typedef void* timer_id_t;
#endif
    
class Timer
{
public:
	timer_id_t idleTimer_ = {};
	int periodMilliSeconds;
	int pendingMs = 0; // elapsed-time accumulator for host-pumped timers (see TimerManager::pump).
	clientContainer_t clients_;

	Timer(int pPeriodMilliSeconds = 50) :
		periodMilliSeconds(pPeriodMilliSeconds)
	{}
	void start();
	void stop();
	void onTimer();
    bool isRunning();
};

}

class TimerManager
{
	std::list< se_sdk_timers::Timer > timers;

public:
	TimerManager();
	~TimerManager();
	static TimerManager* instance();
	void registerClient(TimerClient* client, int periodMilliSeconds);

	void registerClient(TimerClient* client)
	{
		registerClient(client, interval_);
	}
	void unRegisterClient( TimerClient* client );
	void setInterval( int intervalMs );
    void onTimer(se_sdk_timers::timer_id_t timerId);

	// On platforms with no native timer source (e.g. Linux, where Windows
	// SetTimer / macOS CFRunLoopTimer don't exist) the host application must
	// call this periodically on the UI thread, passing the elapsed time.
	// Do NOT call it on Windows/macOS — the native timers already fire there.
	void pump(int elapsedMs);

	// The same thing for a pumper that does not know how long it has been:
	// elapsed time is measured here, from a monotonic clock, so the timers
	// advance at wall-clock rate no matter how often (or how irregularly) this
	// is called.
	//
	// That property is the reason it exists rather than being a convenience.
	// A PLUG-IN has no application loop of its own -- its only UI-thread tick
	// is whatever run loop the host gives it, and there is one of those PER
	// OPEN EDITOR. Two instances of the same plug-in would each call pump(16)
	// every 16 ms, and this manager is a process-wide singleton, so every timer
	// client in the process would run at twice its period. Here the second
	// caller simply observes that no time has passed.
	void pump();

private:
	int interval_;

	// Zero until the first pump(); see pump() in the .cpp for why the first
	// call only starts the clock.
	long long lastPumpMonotonicMs_ = 0;
};

}
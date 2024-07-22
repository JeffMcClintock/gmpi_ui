#pragma once

#include <vector>
#include <list>

/*
This class provides a programmable timer for creating animation effects.
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

private:
	int interval_;
};

}
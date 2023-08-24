#ifndef GMPI_CriticalSection_h_INCLUDED
#define GMPI_CriticalSection_h_INCLUDED
/*
#include "../shared/xp_critical_section.h"
*/

// c++11?
#if __cplusplus >= 199711L && !defined(__llvm__) // llvm does not seem to support <mutex> on Mac Pluginbundle
#define GMPI_USE_STD_LIBRARY_THREADS
#endif

#ifdef GMPI_USE_STD_LIBRARY_THREADS

#include <memory>
#include <mutex>

#else

#ifdef _WIN32
#	include <windows.h>
#else
#	include <unistd.h>
#	include <pthread.h>
#endif


#endif

namespace gmpi_sdk
{

#ifdef GMPI_USE_STD_LIBRARY_THREADS

	class CriticalSectionXp
	{
	public:
		CriticalSectionXp(void)
		{
			m_cSection = std::make_shared< std::mutex >();
		}

		  /**
		  * @fn void Enter(void)
		  * @brief Wait for unlock and enter the CriticalSectionXp object.
		  * @see TryEnter()
		  * @return void
		  */
		void Enter(void)
		{
			m_cSection->lock();
		}

		  /**
		  * @fn void Leave(void)
		  * @brief Leaves the critical section object.
		  * This function will only work if the current thread
		  * holds the current lock on the CriticalSectionXp object
		  * called by Enter()
		  * @see Enter()
		  * @return void
		  */
		void Leave(void)
		{
			m_cSection->unlock();
		}

		  /**
		  * @fn bool TryEnter(void)
		  * @brief Attempt to enter the CriticalSectionXp object
		  * @return bool(true) on success, bool(false) if otherwise
		  */
		bool TryEnter(void)
		{
			return m_cSection->try_lock();
		}

private:

	std::shared_ptr< std::mutex > m_cSection;

}; // class CriticalSectionXp

#else // GMPI_USE_STD_LIBRARY_THREADS

/**
 * @class A wrapper-class around Critical Section functionality, WIN32 & PTHREADS.
 */
class CriticalSectionXp
{
public:
	/**
	 * @brief CriticalSectionXp class constructor.
	 */
	CriticalSectionXp(void)
	{
	#ifdef _WIN32
		#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
			if (0 == InitializeCriticalSectionEx(&m_cSection, 0, 0)) // Vista AND store apps
				throw("Could not create a CriticalSectionXp");
		#else
			InitializeCriticalSection(&m_cSection);               // Desktop only. XP and better.
		//if (0 == InitializeCriticalSectionAndSpinCount(&m_cSection, 0)) // Windows 7 Desktop only.
		//		throw("Could not create a CriticalSectionXp");
		#endif
	#else
		// Mac.
		if (pthread_mutex_init(&m_cSection, NULL) != 0)
			throw("Could not create a CriticalSectionXp");
	#endif
	} // CriticalSectionXp()

	/**
	 * @brief CriticalSectionXp class destructor
	 */
	~CriticalSectionXp(void)
	{
	#ifdef _WIN32
		DeleteCriticalSection(&m_cSection);
	#else
		pthread_mutex_destroy(&m_cSection);
	#endif
	} // ~CriticalSectionXp()

	/**
	 * @fn void Enter(void)
	 * @brief Wait for unlock and enter the CriticalSectionXp object.
	 * @see TryEnter()
	 */
	void Enter(void)
	{
	#ifdef _WIN32
		EnterCriticalSection(&m_cSection);
	#else
		pthread_mutex_lock(&m_cSection);
	#endif
	} // Enter()

	/**
	 * @fn void Leave(void)
	 * @brief Leaves the critical section object.
	 * This function will only work if the current thread
	 * holds the current lock on the CriticalSectionXp object
	 * called by Enter()
	 * @see Enter()
	 * @return void
	 */
	void Leave(void)
	{
	#ifdef _WIN32
		LeaveCriticalSection(&m_cSection);
	#else
		pthread_mutex_unlock(&m_cSection);
	#endif
	} // Leave()

	/**
	 * @fn bool TryEnter(void)
	 * @brief Attempt to enter the CriticalSectionXp object
	 * @return bool(true) on success, bool(false) if otherwise
	 */
	bool TryEnter(void)
	{
		// Attempt to acquire ownership:
	#ifdef _WIN32
        #if(_WIN32_WINNT >= 0x0400)
		    return(TRUE == TryEnterCriticalSection(&m_cSection));
        #else
            EnterCriticalSection(&m_cSection);
            return true;
        #endif
	#else
		return(0 == pthread_mutex_trylock(&m_cSection));
	#endif
	} // TryEnter()

private:

#ifdef GMPI_USE_STD_LIBRARY_THREADS
	std::mutex m_cSection;
#else

#ifdef _WIN32
	CRITICAL_SECTION m_cSection; //!< internal system critical section object (windows)
#else
	pthread_mutex_t m_cSection; //!< internal system critical section object (*nix)
#endif

#endif

}; // class CriticalSectionXp

#endif // GMPI_USE_STD_LIBRARY_THREADS

class AutoCriticalSection
{
public:
	explicit AutoCriticalSection(CriticalSectionXp& lock) : lock_(lock)
    {
        lock_.Enter();
    }
    ~AutoCriticalSection()
    {
        lock_.Leave();
    }

private:
	// copy ops are private to prevent copying
	AutoCriticalSection(const AutoCriticalSection&);
	AutoCriticalSection& operator=(const AutoCriticalSection&);

	CriticalSectionXp& lock_;
};

}

#endif

#ifndef __UTILS_H__
#define __UTILS_H__

#ifdef _WIN32
# include <windows.h>
#else
# include <pthread.h>
#endif

enum OSEventError {
	OSEvent_Error_Unsupported = -1,
	OSEvent_Error_NotInitialized = -2,
	OSEvent_Error_TimedOut = -3,
	OSEvent_Error_Unknown = -4,
	OSEvent_Error_None = 0,
};

class OSEvent
{
public:
	OSEvent(int &sts, bool manual, bool state);
	~OSEvent(void);

	void Signal(void);
	void Reset(void);
	void Wait(void);
	OSEventError TimedWait(unsigned msec);

private:
#ifdef _WIN32
	void* m_event;
#else
	bool m_manual;
	bool m_state;
	pthread_cond_t m_event;
	pthread_mutex_t m_mutex;
#endif
};

#endif

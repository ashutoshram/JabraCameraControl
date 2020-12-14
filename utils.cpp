#include "utils.h"
#include <sys/time.h>
#include <errno.h>

OSEvent::OSEvent(int &sts, bool manual, bool state)
{
#ifdef _WIN32
	sts = 0;
	m_event = CreateEvent(NULL, manual, state, NULL);
	if (!m_event) sts = -1;
#else
	sts = 0;

	m_manual = manual;
	m_state = state;
	pthread_cond_init(&m_event, NULL);
	pthread_mutex_init(&m_mutex, NULL);
#endif
}

OSEvent::~OSEvent(void)
{
#ifdef _WIN32
	if (m_event) CloseHandle(m_event);
#else
	pthread_cond_destroy(&m_event);
	pthread_mutex_destroy(&m_mutex);
#endif
}

void OSEvent::Signal(void)
{
#ifdef _WIN32
	if (m_event) SetEvent(m_event);
#else
	int res = pthread_mutex_lock(&m_mutex);
	if (!res)
	{
		if (!m_state)
		{
			m_state = true;
			if (m_manual) pthread_cond_broadcast(&m_event);
			else pthread_cond_signal(&m_event);
		}
		res = pthread_mutex_unlock(&m_mutex);
	}
#endif
}

void OSEvent::Reset(void)
{
#ifdef _WIN32
	if (m_event) ResetEvent(m_event);
#else
	int res = pthread_mutex_lock(&m_mutex);
	if (!res)
	{
		if (m_state) m_state = false;
		res = pthread_mutex_unlock(&m_mutex);
	}
#endif
}

void OSEvent::Wait(void)
{
#ifdef _WIN32
	if (m_event) WaitForSingleObject(m_event, INFINITE);
#else
	int res = pthread_mutex_lock(&m_mutex);
	if (!res)
	{
		while (!m_state) pthread_cond_wait(&m_event, &m_mutex); // take care of spurious waits
		if (!m_manual) m_state = false;
		res = pthread_mutex_unlock(&m_mutex);
	}
#endif
}


OSEventError OSEvent::TimedWait(unsigned msec)
{
#ifdef _WIN32
	if (INFINITE == msec) return OSEvent_Error_Unsupported;
	OSEventError res = OSEvent_Error_NotInitialized;
	if (m_event)
	{
		DWORD res1 = WaitForSingleObject(m_event, msec);
		if (WAIT_OBJECT_0 == res1) {
			res = OSEvent_Error_None;
		}
		else if (WAIT_TIMEOUT == res1) {
			res = OSEvent_Error_TimedOut;
		}
		else {
			res = OSEvent_Error_Unknown;
		}
	}

	return res;
#else
	if (0xFFFFFFFF == msec) return OSEvent_Error_Unsupported;
	OSEventError res = OSEvent_Error_NotInitialized;

    int res1 = pthread_mutex_lock(&m_mutex);
	if (!res1)
	{
		if (!m_state)
		{
			struct timeval tval;
			struct timespec tspec;

			gettimeofday(&tval, NULL);
			long usec = 1000 * msec + tval.tv_usec;
			tspec.tv_sec = tval.tv_sec + usec / 1000000;
			tspec.tv_nsec = (usec % 1000000) * 1000;
			res1 = pthread_cond_timedwait(&m_event,
				&m_mutex,
				&tspec);
			if (0 == res1 && m_state) res = OSEvent_Error_None;
			else if (ETIMEDOUT == res1) res = OSEvent_Error_TimedOut;
			else res = OSEvent_Error_Unknown;
		}
		else res = OSEvent_Error_None;
		if(res == OSEvent_Error_None && m_state && !m_manual)
            m_state = false;

		res1 = pthread_mutex_unlock(&m_mutex);
		if (res1) res = OSEvent_Error_Unknown;
	}
	else res = OSEvent_Error_Unknown;

	return res;
#endif
}


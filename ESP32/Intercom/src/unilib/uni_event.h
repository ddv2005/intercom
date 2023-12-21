/*
 * uni_event.h
 *
 *  Created on: Mar 11, 2013
 *      Author: ddv
 */

#ifndef UNI_EVENT_H_
#define UNI_EVENT_H_

#include "uni_common.h"
#ifdef __POSIX
#include <pthread.h>
#include <sys/time.h>
#endif
#include <errno.h>

class uni_event_base
{
public:
	virtual ~uni_event_base(){};
	virtual void wait()=0;
	virtual int wait(int32_t timeout)=0;
	virtual void signal()=0;
	virtual void reset(){};
};

#ifdef __POSIX
class uni_event:public uni_event_base
{
protected:
	pthread_mutex_t m_mutex;
	pthread_cond_t m_cond;
	bool m_triggered;
	bool monotonic;
public:
	uni_event()
	{
		pthread_condattr_t cond_attr;
		pthread_mutex_init(&m_mutex, 0);
		pthread_condattr_init(&cond_attr);
		monotonic = pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC)==0;
		pthread_cond_init(&m_cond, &cond_attr);
#ifndef ESP32		
		pthread_condattr_destroy(&cond_attr);
#endif		
		m_triggered = false;
	}
	virtual ~uni_event()
	{
		pthread_cond_destroy(&m_cond);
		pthread_mutex_destroy(&m_mutex);
	}

	virtual void wait()
	{
		pthread_mutex_lock(&m_mutex);
		while (!m_triggered)
			pthread_cond_wait(&m_cond, &m_mutex);
		m_triggered = false;
		pthread_mutex_unlock(&m_mutex);
	}

	virtual int wait(int32_t timeout)
	{
		pthread_mutex_lock(&m_mutex);

		struct timespec timeToWait;

		if(monotonic)
			clock_gettime(CLOCK_MONOTONIC, &timeToWait);
		else
			clock_gettime(CLOCK_REALTIME, &timeToWait);

		timeToWait.tv_sec  += timeout / 1000;
		timeToWait.tv_nsec += 1000000UL * ((int64_t)timeout % 1000);
		timeToWait.tv_sec  += timeToWait.tv_nsec / 1000000000UL;
		timeToWait.tv_nsec = timeToWait.tv_nsec % 1000000000UL;

		int err = 0;
		while (!m_triggered)
		{
			err = pthread_cond_timedwait(&m_cond, &m_mutex, &timeToWait);
			if (err == 0 || err == ETIMEDOUT)
				break;
			if (err == EINVAL)
			{
				err = 0;
				break;
			}
		}
		if (!err)
			m_triggered = false;
		pthread_mutex_unlock(&m_mutex);
		return err;
	}

	virtual void signal()
	{
		pthread_mutex_lock(&m_mutex);
		m_triggered = true;
		pthread_cond_signal(&m_cond);
		pthread_mutex_unlock(&m_mutex);
	}
};

class uni_shared_event:public uni_event_base
{
protected:
	pthread_mutex_t m_mutex;
	pthread_cond_t m_cond;
	bool m_autoReset;
	bool m_triggered;
	bool monotonic;
public:
	uni_shared_event(bool autoReset=true):m_autoReset(autoReset)
	{
		m_triggered = false;
		pthread_condattr_t cond_attr;
		pthread_mutex_init(&m_mutex, 0);
		pthread_condattr_init(&cond_attr);
		monotonic = pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC)==0;
		pthread_cond_init(&m_cond, &cond_attr);
#ifndef ESP32		
		pthread_condattr_destroy(&cond_attr);
#endif		
	}
	~uni_shared_event()
	{
		pthread_cond_destroy(&m_cond);
		pthread_mutex_destroy(&m_mutex);
	}

	virtual void wait()
	{
		pthread_mutex_lock(&m_mutex);
		if(!m_triggered)
			pthread_cond_wait(&m_cond, &m_mutex);
		if(m_autoReset)
			m_triggered = false;
		pthread_mutex_unlock(&m_mutex);
	}

	virtual int wait(int32_t timeout)
	{
		int err = 0;
		pthread_mutex_lock(&m_mutex);
		if(!m_triggered)
		{
			struct timespec timeToWait;

			if(monotonic)
				clock_gettime(CLOCK_MONOTONIC, &timeToWait);
			else
				clock_gettime(CLOCK_REALTIME, &timeToWait);

			timeToWait.tv_sec  += timeout / 1000;
			timeToWait.tv_nsec += 1000000UL * ((int64_t)timeout % 1000);
			timeToWait.tv_sec  += timeToWait.tv_nsec / 1000000000UL;
			timeToWait.tv_nsec = timeToWait.tv_nsec % 1000000000UL;

			err = pthread_cond_timedwait(&m_cond, &m_mutex, &timeToWait);
		}
		if (!err && m_autoReset)
			m_triggered = false;
		pthread_mutex_unlock(&m_mutex);
		return err;
	}

	virtual void signal()
	{
		pthread_mutex_lock(&m_mutex);
		m_triggered = true;
		pthread_cond_broadcast(&m_cond);
		pthread_mutex_unlock(&m_mutex);
	}

	virtual void reset()
	{
		pthread_mutex_lock(&m_mutex);
		m_triggered = false;
		pthread_mutex_unlock(&m_mutex);
	}

};
#endif

#ifdef __WINDOWS
class uni_event_nt_base:public uni_event_base
{
protected:
	HANDLE m_event;
	bool m_manual;
	bool m_autoReset;
public:
	uni_event_nt_base(bool manual_=false,bool autoReset=true):m_manual(manual_),m_autoReset(autoReset)
	{
		m_event = CreateEvent(NULL,m_manual,false,NULL);
	}
	virtual ~uni_event_nt_base()
	{
		CloseHandle(m_event);
	}

	virtual void wait()
	{
		WaitForSingleObject(m_event,INFINITE);
	}

	virtual int wait(int32_t timeout)
	{
		int result = WaitForSingleObject(m_event,timeout);

		switch(result)
		{
		case WAIT_OBJECT_0:
			return 0;
		case WAIT_TIMEOUT:
			return ETIMEDOUT;
		default:
			return EINVAL;
		};
	}

	virtual void signal()
	{
		SetEvent(m_event);
		if(m_manual&&m_autoReset)
			ResetEvent(m_event);
	}

	virtual void reset()
	{
		ResetEvent(m_event);
	}
};

class uni_event:public uni_event_nt_base
{
public:
	uni_event():uni_event_nt_base(false){}
	virtual ~uni_event(){}
};

class uni_shared_event:public uni_event_nt_base
{
public:
	uni_shared_event(bool autoReset=true):uni_event_nt_base(true,autoReset){}
	virtual ~uni_shared_event(){}
};
#endif

#endif /* UNI_EVENT_H_ */

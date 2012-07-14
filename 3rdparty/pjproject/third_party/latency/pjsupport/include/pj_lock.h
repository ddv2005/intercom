#ifndef __PJ_LOCK_H
#define	__PJ_LOCK_H

#include "pj_support.h"
#include <pthread.h>
#include <sys/time.h>
#include <errno.h>

//#define	DEBUG_LOCKS	1

#if DEBUG_LOCKS==1
#include <queue>
#include <string>
#define	ACQUIRE_PARAMS	const char* file,const int line
#define	ACQUIRE_PARAMS_S ,ACQUIRE_PARAMS
#define	ACQUIRE_PARAMS_LIST	file,line
#define	ACQUIRE_DATA	__FILE__,__LINE__
#define	ACQUIRE_DATA_S	,ACQUIRE_DATA
#else
#define	ACQUIRE_PARAMS
#define	ACQUIRE_PARAMS_S
#define	ACQUIRE_DATA
#define	ACQUIRE_PARAMS_LIST
#define	ACQUIRE_DATA_S
#endif


class PPJ_Lock
{
protected:
	pj_lock_t *m_lock;
#if DEBUG_LOCKS==1
#define LOCK_OWNER_SIZE	128
	typedef	std::queue<std::string>		lock_owners_t;

	lock_owners_t	m_owners;
	pj_lock_t		*m_olock;

#endif
public:
	inline PPJ_Lock(pj_pool_t *pool)
	{
#if DEBUG_LOCKS==1
		pj_lock_create_semaphore(pool,NULL,1,1,&m_olock);
#endif
	}

	~PPJ_Lock()
	{
		PJS_ASSERT(pj_lock_destroy(m_lock)==PJ_SUCCESS);
#if DEBUG_LOCKS==1
		PJS_ASSERT(pj_lock_destroy(m_olock)==PJ_SUCCESS);
#endif
	}

	inline pj_status_t acquire(ACQUIRE_PARAMS)
	{
#if DEBUG_LOCKS==1
		pj_status_t result = pj_lock_acquire(m_lock);
		char owner[128];
		snprintf(owner,sizeof(owner),"acquire at %s:%d",file,line);
		pj_lock_acquire(m_olock);
		m_owners.push(owner);
		pj_lock_release(m_olock);
		return result;
#else
		return pj_lock_acquire(m_lock);
#endif
	}

#if DEBUG_LOCKS==1
	inline pj_status_t debug_acquire(ACQUIRE_PARAMS)
	{
		pj_status_t result;
		for(int i=0; i<10; i++)
		{
			result = pj_lock_tryacquire(m_lock);
			if(result==PJ_SUCCESS)
			{
				char owner[128];
				snprintf(owner,sizeof(owner),"acquire at %s:%d",file,line);
				m_owners.push(owner);
				return result;
			}
			else
				sched_yield();
		}
		PJ_LOG(1,("LOCKS","ERROR! STUCK AT %s:%d",file,line));
		pj_lock_acquire(m_olock);
		if(m_owners.size()>0)
		{
			PJ_LOG(1,("LOCKS","ERROR! STUCK. LOCKED AT %s",m_owners.front().c_str()));
		}
		pj_lock_release(m_olock);
		return acquire(ACQUIRE_DATA);
	}
#else
	inline pj_status_t debug_acquire()
	{
		return acquire();
	}
#endif

	inline pj_status_t try_acquire()
	{
		return pj_lock_tryacquire(m_lock);
	}

	inline pj_status_t release()
	{
#if DEBUG_LOCKS==1
		pj_lock_acquire(m_olock);
		m_owners.pop();
		pj_lock_release(m_olock);
#endif
		return pj_lock_release(m_lock);
	}
};

class PPJ_MutexLock:public PPJ_Lock
{
public:
	inline PPJ_MutexLock(pj_pool_t *pool,const char *name):PPJ_Lock(pool)
	{
		PJS_ASSERT(pj_lock_create_recursive_mutex(pool,name,&m_lock)==PJ_SUCCESS);
	}
};

class PPJ_SemaphoreLock:public PPJ_Lock
{
public:
	inline PPJ_SemaphoreLock(pj_pool_t *pool,const char *name,unsigned initial, unsigned max):PPJ_Lock(pool)
	{
		PJS_ASSERT(pj_lock_create_semaphore(pool,name,initial,max,&m_lock)==PJ_SUCCESS);
	}
};

class PPJ_WaitAndLock
{
protected:
	PPJ_Lock &m_lock;
public:
	inline PPJ_WaitAndLock(PPJ_Lock &lock ACQUIRE_PARAMS_S):m_lock(lock)
	{
		PJS_ASSERT(m_lock.acquire(ACQUIRE_PARAMS_LIST)==PJ_SUCCESS);
	}

	inline ~PPJ_WaitAndLock()
	{
		PJS_ASSERT(m_lock.release()==PJ_SUCCESS);
	}
};

class PPJ_Event
{
protected:
	pthread_mutex_t m_mutex;
	pthread_cond_t m_cond;
	bool m_triggered;
public:
	PPJ_Event()
	{
		pthread_mutex_init(&m_mutex, 0);
		pthread_cond_init(&m_cond, 0);
		m_triggered = false;
	}
	~PPJ_Event()
	{
		pthread_mutex_destroy(&m_mutex);
		pthread_cond_destroy(&m_cond);
	}

	void wait() {
	     pthread_mutex_lock(&m_mutex);
	     while (!m_triggered)
	         pthread_cond_wait(&m_cond, &m_mutex);
	     m_triggered = false;
	     pthread_mutex_unlock(&m_mutex);
	}

	void wait(pj_int32_t timeout) {
	     pthread_mutex_lock(&m_mutex);
	     struct timespec abs_time;
	     struct timeval now;

	     gettimeofday(&now,NULL);
	     abs_time.tv_sec = now.tv_sec+timeout/1000;
	     abs_time.tv_nsec = (now.tv_usec+1000UL*(timeout%1000))*1000UL;

	     int err = 0;
	     while (!m_triggered)
	     {
	         err = pthread_cond_timedwait(&m_cond, &m_mutex,&abs_time);
	         if(err == 0 || err == ETIMEDOUT)
	        	 break;
	         if(err==EINVAL)
	        	 abort();
	     }
	     if(!err)
	    	 m_triggered = false;
	     pthread_mutex_unlock(&m_mutex);
	}

	void signal() {
	    pthread_mutex_lock(&m_mutex);
	    m_triggered = true;
	    pthread_cond_signal(&m_cond);
	    pthread_mutex_unlock(&m_mutex);
	}
};

#endif


#ifndef UNI_MUTEX_H_
#define UNI_MUTEX_H_

#include "uni_common.h"
#include "uni_lock.h"

#ifdef __POSIX
#include <pthread.h>
class uni_simple_mutex:public uni_lock
{
protected:
	pthread_mutex_t m_lock;

	int raw_lock()
	{
		return pthread_mutex_lock(&m_lock);
	}

	int raw_unlock()
	{
		return pthread_mutex_unlock(&m_lock);
	}

public:
	uni_simple_mutex()
	{
		pthread_mutexattr_t   mta;
		pthread_mutexattr_init(&mta);
		pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&m_lock, &mta);
	}

	virtual ~uni_simple_mutex()
	{
		pthread_mutex_destroy(&m_lock);
	}

	virtual void lock()
	{
		raw_lock();
	}

	virtual void unlock()
	{
		raw_unlock();
	}

};
#endif

#ifdef __WINDOWS
class uni_simple_mutex:public uni_lock
{
protected:
	CRITICAL_SECTION m_lock;

	int raw_lock()
	{
		EnterCriticalSection(&m_lock);
		return 0;
	}

	int raw_unlock()
	{
		LeaveCriticalSection(&m_lock);
		return 0;
	}

public:
	uni_simple_mutex()
	{
		InitializeCriticalSection(&m_lock);
	}

	virtual ~uni_simple_mutex()
	{
		DeleteCriticalSection(&m_lock);
	}

	virtual void lock()
	{
		raw_lock();
	}

	virtual void unlock()
	{
		raw_unlock();
	}

};
#endif
#endif /* UNI_MUTEX_H_ */

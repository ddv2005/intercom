#ifndef PJS_UTILS_H_
#define PJS_UTILS_H_
#include "project_common.h"
#include <pthread.h>
#include <list>


class pjs_event
{
protected:
	pthread_mutex_t m_mutex;
	pthread_cond_t m_cond;
	bool m_triggered;
public:
	pjs_event()
	{
		pthread_mutex_init(&m_mutex, 0);
		pthread_cond_init(&m_cond, 0);
		m_triggered = false;
	}
	~pjs_event()
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

	int wait(pj_int32_t timeout) {
	     pthread_mutex_lock(&m_mutex);
	     struct timespec abs_time;
	     struct timeval now;

	     gettimeofday(&now,NULL);
	     abs_time.tv_nsec = (now.tv_usec+1000UL*(timeout%1000))*1000UL;
	     abs_time.tv_sec = now.tv_sec+timeout/1000+abs_time.tv_nsec/1000000000UL;
	     abs_time.tv_nsec %= 1000000000UL;

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
	     return err;
	}

	void signal() {
	    pthread_mutex_lock(&m_mutex);
	    m_triggered = true;
	    pthread_cond_signal(&m_cond);
	    pthread_mutex_unlock(&m_mutex);
	}
};


class pjs_shared_event
{
protected:
	pthread_mutex_t m_mutex;
	pthread_cond_t m_cond;
public:
	pjs_shared_event()
	{
		pthread_mutex_init(&m_mutex, 0);
		pthread_cond_init(&m_cond, 0);
	}
	~pjs_shared_event()
	{
		pthread_mutex_destroy(&m_mutex);
		pthread_cond_destroy(&m_cond);
	}

	void wait() {
	     pthread_mutex_lock(&m_mutex);
         pthread_cond_wait(&m_cond, &m_mutex);
	     pthread_mutex_unlock(&m_mutex);
	}

	int wait(pj_int32_t timeout) {
	     pthread_mutex_lock(&m_mutex);
	     struct timespec abs_time;
	     struct timeval now;

	     gettimeofday(&now,NULL);
	     abs_time.tv_nsec = (now.tv_usec+1000UL*(timeout%1000))*1000UL;
	     abs_time.tv_sec = now.tv_sec+timeout/1000+abs_time.tv_nsec/1000000000UL;
	     abs_time.tv_nsec %= 1000000000UL;

	     int err = pthread_cond_timedwait(&m_cond, &m_mutex,&abs_time);
	     pthread_mutex_unlock(&m_mutex);
	     return err;
	}

	void signal() {
	    pthread_mutex_lock(&m_mutex);
	    pthread_cond_broadcast(&m_cond);
	    pthread_mutex_unlock(&m_mutex);
	}
};

class pjs_simple_mutex
{
protected:
	pthread_mutex_t	m_lock;

	int raw_lock()
	{
		return pthread_mutex_lock(&m_lock);
	}

	int raw_unlock()
	{
		return pthread_mutex_unlock(&m_lock);
	}

	void check_error(int error)
	{
		if(error)
		{
			abort();
		}
	}

public:
	pjs_simple_mutex()
	{
		pthread_mutex_init(&m_lock,NULL);
	}

	~pjs_simple_mutex()
	{
		pthread_mutex_destroy(&m_lock);
	}

	void lock()
	{
		check_error(raw_lock());
	}

	void unlock()
	{
		check_error(raw_unlock());
	}

};

class pjs_simple_locker
{
protected:
	pjs_simple_mutex &m_lock;
public:
	pjs_simple_locker(pjs_simple_mutex &lock):m_lock(lock){m_lock.lock();}
	~pjs_simple_locker(){m_lock.unlock();}
};

SWIG_BEGIN_DECL
template<class T>
class pjs_list
{
protected:
	unsigned int		m_max_elements;
	pjs_simple_mutex	m_lock;
	pjs_event			m_event;
	std::list<T>		m_list;
public:
#ifdef SWIG
protected:
#endif
	pjs_list(unsigned int	max_elements):m_max_elements(max_elements){}
	virtual ~pjs_list() {}
#ifdef SWIG
public:
#endif

	void push(T &v,bool at_front=false)
	{
		pjs_simple_locker lock(m_lock);
		if(m_max_elements)
		{
			while(m_list.size()>=m_max_elements)
				m_list.pop_front();
		}
		if(at_front)
			m_list.push_front(v);
		else
			m_list.push_back(v);
		m_event.signal();
	}

	bool pop(T *v)
	{
		bool result = false;
		if(v)
		{
			pjs_simple_locker lock(m_lock);
			if(!m_list.empty())
			{
				*v = m_list.front();
				m_list.pop_front();
				result = true;
			}
		}
		return result;
	}

	bool empty()
	{
		pjs_simple_locker lock(m_lock);
		return m_list.empty();
	}

	void cancel_wait()
	{
		m_event.signal();
	}

	int wait(pj_int32_t timeout)
	{
		m_lock.lock();
		if(m_list.empty())
		{
			m_lock.unlock();
			return m_event.wait(timeout);
		}
		m_lock.unlock();
		return 0;
	}

	void clear()
	{
		pjs_simple_locker lock(m_lock);
		m_list.clear();
	}
};
SWIG_END_DECL

#endif /* PJS_UTILS_H_ */

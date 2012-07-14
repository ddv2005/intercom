#ifndef PJS_GLOBAL_H_
#define PJS_GLOBAL_H_
#include "project_common.h"
#include "project_config.h"
#include "pj_support.h"

#define NO_IOTHREAD	1

class pjs_global
{
protected:
	pjs_config_t  	*m_config;
	pj_pool_factory *m_pool_factory;
	pj_pool_t		*m_pool;
	pj_thread_t 	*m_timer_thread;
#ifndef	NO_IOTHREAD
	pj_thread_t 	*m_io_thread;
	pj_ioqueue_t	*m_ioqueue;
#endif
	pj_timer_heap_t *m_timer_heap;
	bool			 m_exit;
	PPJ_MutexLock	*m_timer_lock;
	PPJ_MutexLock	*m_config_lock;

	static void* timer_thread_proc_(pjs_global *global)
	{
		if(global)
			return global->timer_thread_proc();
		else
			return NULL;
	}

	void* timer_thread_proc()
	{
		//set_max_relative_thread_priority(-10);
		PJ_LOG_(DEBUG_LEVEL,(__FILE__,"timer_thread_proc start"));
		if(m_timer_heap)
		{
		    while (!m_exit)
		    {
		    	pj_time_val delay = {0, 50};
		    	m_timer_lock->acquire(ACQUIRE_DATA);
		    	pj_timer_heap_poll(m_timer_heap, &delay);
		    	m_timer_lock->release();
		    	pj_thread_sleep(20);
		    }
	    }
		PJ_LOG_(DEBUG_LEVEL,(__FILE__,"timer_thread_proc stop"));
		return NULL;
	}
#ifndef	NO_IOTHREAD
	static void* io_thread_proc_(pjs_global *global)
	{
		if(global)
			return global->io_thread_proc();
		else
			return NULL;
	}

	void* io_thread_proc()
	{
		set_max_relative_thread_priority(-5);
		PJ_LOG_(DEBUG_LEVEL,(__FILE__,"io_thread_proc start"));
		if(m_ioqueue)
		{
		    while (!m_exit)
		    {
		    	pj_time_val delay = {0, 100};
		    	pj_ioqueue_poll(m_ioqueue, &delay);
		    }
	    }
		PJ_LOG_(DEBUG_LEVEL,(__FILE__,"io_thread_proc stop"));
		return NULL;
	}
#endif
public:
	pjs_global(pj_pool_factory *pool_factory,pjs_config_t *config):m_pool_factory(pool_factory)
	{
		pj_status_t status;
		m_config = config;
#ifndef	NO_IOTHREAD
		m_io_thread = NULL;
#endif
		m_timer_thread = NULL;
		m_pool = pj_pool_create(m_pool_factory,"pjs_global",512,512,NULL);
		m_exit = false;
		m_timer_lock = new PPJ_MutexLock(m_pool,NULL);
		m_config_lock = new PPJ_MutexLock(m_pool,NULL);
#ifndef	NO_IOTHREAD
		status = pj_ioqueue_create(m_pool,64,&m_ioqueue);
		if(status==PJ_SUCCESS)
		{
#endif
			status = pj_timer_heap_create(m_pool,16,&m_timer_heap);
			if(status==PJ_SUCCESS)
			{
				pj_lock_t *lock;
				status = pj_lock_create_recursive_mutex( m_pool, "timers_lock%p", &lock);
				if (status == PJ_SUCCESS)
				{
					pj_timer_heap_set_lock(m_timer_heap,lock,PJ_TRUE);

					status = pj_thread_create(m_pool, "timer thread", (pj_thread_proc*)&timer_thread_proc_,
								  this,
								  PJ_THREAD_DEFAULT_STACK_SIZE,
								  0,
								  &m_timer_thread);
					if(status==PJ_SUCCESS)
					{
#ifndef	NO_IOTHREAD
						status = pj_thread_create(m_pool, "io thread", (pj_thread_proc*)&io_thread_proc_,
									  this,
									  PJ_THREAD_DEFAULT_STACK_SIZE,
									  0,
									  &m_io_thread);
						if (status != PJ_SUCCESS)
						{
							m_io_thread = NULL;
							PJ_LOG_(ERROR_LEVEL,(__FILE__,"pj_thread_create error %d",status));
						}
#endif
					}
					else
					{
						m_timer_thread = NULL;
						PJ_LOG_(ERROR_LEVEL,(__FILE__,"pj_thread_create error %d",status));
					}
				}
				else
					PJ_LOG_(ERROR_LEVEL,(__FILE__,"pj_lock_create_recursive_mutex error %d",status));

			}
			else
			{
				m_timer_heap = NULL;
				PJ_LOG_(ERROR_LEVEL,(__FILE__,"pj_timer_heap_create error %d",status));
			}
#ifndef	NO_IOTHREAD
		}
		else
		{
			m_ioqueue = NULL;
			PJ_LOG_(ERROR_LEVEL,(__FILE__,"pj_ioqueue_create error %d",status));
		}
#endif
	}

	~pjs_global()
	{
		m_exit = true;
		if(m_timer_thread)
		{
			pj_thread_join(m_timer_thread);
			pj_thread_destroy(m_timer_thread);
			m_timer_thread = NULL;
		}
#ifndef	NO_IOTHREAD
		if(m_io_thread)
		{
			pj_thread_join(m_io_thread);
			pj_thread_destroy(m_io_thread);
			m_io_thread = NULL;
		}
#endif
		if(m_timer_heap)
			pj_timer_heap_destroy(m_timer_heap);
#ifndef	NO_IOTHREAD
		if(m_ioqueue)
			pj_ioqueue_destroy(m_ioqueue);
#endif
		delete m_config_lock;
		delete m_timer_lock;
		pj_pool_release(m_pool);
	}

	bool is_ok()
	{
		return (m_timer_heap != NULL)
#ifndef	NO_IOTHREAD
				&& (m_ioqueue != NULL)
#endif
				&& (m_timer_thread != NULL);
	}
	pjs_config_t &get_config() { return *m_config; }
	pj_pool_factory *get_pool_factory() { return m_pool_factory; }
	pj_timer_heap_t *get_timer_heap() { return m_timer_heap; }
#ifndef	NO_IOTHREAD
	pj_ioqueue_t	*get_ioqueue() { return m_ioqueue; }
#endif
	void lock_timer_thread() { m_timer_lock->acquire(ACQUIRE_DATA); }
	void unlock_timer_thread() { m_timer_lock->release(); }
	void lock_config() { m_config_lock->acquire(ACQUIRE_DATA); }
	void unlock_config() { m_config_lock->release(); }
};


#endif /* PJS_GLOBAL_H_ */

#ifndef UNI_THREADS_POOL_H_
#define UNI_THREADS_POOL_H_

#include "uni_common.h"
#include "uni_lock.h"
#include "uni_mutex.h"
#include "uni_event.h"
#include "uni_thread.h"

#include <list>

class uni_threads_pool;
class uni_pool_internal_thread;

class uni_pool_thread: public uni_thread_base
{
	friend class uni_pool_internal_thread;
protected:
	uni_threads_pool *m_pool;
	uni_shared_event m_endEvent;
	uni_pool_internal_thread *m_realThread;

	virtual int internalStart();
	virtual void internalWaitForTermination();
	virtual void internalOnThreadExit();
	virtual uni_thread_id getThreadID();
public:
	uni_pool_thread(uni_threads_pool *pool);
	uni_pool_thread(uint32_t stackSize = 0);
	void setThreadsPool(uni_threads_pool *pool);
	virtual ~uni_pool_thread();
	void setUserData(void *userData);
};

class uni_pool_internal_thread: public uni_thread_with_signal_nd
{
	friend class uni_threads_pool;
	friend class uni_pool_thread;
protected:
	uni_threads_pool *m_pool;
	uni_pool_thread *m_poolThread;
	uint32_t				 m_idleTimeout;
	void					  *m_userData;
	bool startPoolThread();
	uni_pool_internal_thread(uni_threads_pool *pool, uint32_t idleTimeout, uint32_t stackSize = 0);
	bool isAvailable() { return (m_thread!=0)&&(!m_exit); }
public:
	virtual ~uni_pool_internal_thread();
	virtual void run();
};

class uni_threads_pool
{
	friend class uni_pool_thread;
	friend class uni_pool_internal_thread;
protected:
	typedef std::list<uni_pool_internal_thread *> athreads_list_t;
	typedef std::list<uni_pool_internal_thread *>::iterator athreads_list_itr_t;
	uni_simple_mutex m_lock;
	uint32_t m_idleTimeout;
	uint32_t m_stackSize;
	athreads_list_t m_availableThreadsList;
	uni_pool_internal_thread *popRealThread(uni_pool_thread *poolThread);
	void pushRealThread(uni_pool_internal_thread *thread);
	bool closeRealThread(uni_pool_internal_thread *thread);
	void closeAvailableThreads();
public:
	uni_threads_pool(uint32_t idleTimeout = 180000, uint32_t stackSize = 0);
	virtual ~uni_threads_pool();
};

#endif /* UNI_THREADS_POOL_H_ */

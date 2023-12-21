#include "uni_threads_pool.h"
#include <stdlib.h>

uni_pool_thread::uni_pool_thread(uni_threads_pool *pool) :m_pool(pool),
		m_endEvent(false)
{
	m_realThread = NULL;
}

uni_pool_thread::uni_pool_thread(uint32_t stackSize):m_pool(NULL),m_endEvent(false),m_realThread(NULL)
{
}

void uni_pool_thread::setUserData(void *userData)
{
	m_realThread->m_userData = userData;
}

void uni_pool_thread::setThreadsPool(uni_threads_pool *pool)
{
	m_pool = pool;
}

uni_pool_thread::~uni_pool_thread()
{
	onDestroy();
}

uni_thread_id uni_pool_thread::getThreadID()
{
  if(m_realThread)
  	return m_realThread->getThreadID();
  return 0;
}

void uni_pool_thread::internalWaitForTermination()
{
	bool b = m_realThread!=NULL;
	m_lock.unlock();
	if(b)
		m_endEvent.wait();
	m_lock.lock();
}
void uni_pool_thread::internalOnThreadExit()
{
	m_realThread = NULL;
	m_endEvent.signal();
}

int uni_pool_thread::internalStart()
{
	if (!m_realThread&&m_pool)
	{
		m_realThread = m_pool->popRealThread(this);
		m_endEvent.reset();
		if (m_realThread->startPoolThread())
			m_state = utsRunning;
	}
	return m_state == utsRunning ? URC_OK : URC_ERROR;
}

bool uni_pool_internal_thread::startPoolThread()
{
	bool result = false;
	if (m_poolThread && !m_exit)
	{
		m_signal.signal();
		result = true;
	}
	return result;
}

uni_pool_internal_thread::uni_pool_internal_thread(uni_threads_pool *pool, uint32_t idleTimeout, uint32_t stackSize ):uni_thread_with_signal_nd(stackSize),
		m_pool(pool),m_poolThread(NULL),m_idleTimeout(idleTimeout),m_userData(NULL)
{
}

uni_pool_internal_thread::~uni_pool_internal_thread()
{
	onDestroy();
	if(m_userData)
		free(m_userData);
}

void uni_pool_internal_thread::run()
{
	while (!m_exit)
	{
		if (!m_exit)
		{
			m_signal.wait(m_idleTimeout);
			if(!m_poolThread&&!m_exit&&m_pool)
			{
				if(m_pool->closeRealThread(this))
					break;
			}
		}
		if (!m_exit && m_poolThread)
		{
			try
			{
				m_poolThread->run();
			} catch (...)
			{
				m_exit = true;
			}
			bool deleteOnClose = m_poolThread->m_deleteOnClose;
			m_poolThread->onThreadExit();
			if(deleteOnClose)
				delete m_poolThread;
		}
		m_poolThread = NULL;
		if (!m_exit)
		{
			if (m_pool)
				m_pool->pushRealThread(this);
			else
				m_exit = true;
		}
	}
}

uni_threads_pool::uni_threads_pool(uint32_t idleTimeout, uint32_t stackSize):m_idleTimeout(idleTimeout),m_stackSize(stackSize)
{
}

uni_threads_pool::~uni_threads_pool()
{
	closeAvailableThreads();
}


uni_pool_internal_thread *uni_threads_pool::popRealThread(uni_pool_thread *poolThread)
{
	uni_pool_internal_thread *result=NULL;
	uni_locker lock(m_lock);
	while(m_availableThreadsList.size())
	{
		uni_pool_internal_thread *thread = m_availableThreadsList.front();
		m_availableThreadsList.pop_front();
		if(thread->isAvailable())
		{
			result = thread;
			break;
		}
	}
	if(!result)
	{
		result = new uni_pool_internal_thread(this,m_idleTimeout,m_stackSize);
		if(result)
		{
			result->setDeleteOnClose(true);
			if(result->start()!=URC_OK)
			{
				delete result;
				result = NULL;
			}
		}
	}

	if(result)
	{
		result->m_poolThread = poolThread;
		result->setDeleteOnClose(true);
	}

	return result;
}

bool uni_threads_pool::closeRealThread(uni_pool_internal_thread *thread)
{
	uni_locker lock(m_lock);
	bool result = !thread->m_poolThread;
	if(result)
	{
		m_availableThreadsList.remove(thread);
		thread->setDeleteOnClose(true);
  }
	return result;
}

void uni_threads_pool::pushRealThread(uni_pool_internal_thread *thread)
{
	uni_locker lock(m_lock);
	if(!thread->isStopped())
	{
		thread->setDeleteOnClose(false);
		m_availableThreadsList.push_back(thread);
	}
}


void uni_threads_pool::closeAvailableThreads()
{
	uni_locker lock(m_lock);
	for(athreads_list_itr_t itr=m_availableThreadsList.begin();itr!=m_availableThreadsList.end();itr++)
		(*itr)->stop();
	while(m_availableThreadsList.size())
	{
		uni_pool_internal_thread *thread = m_availableThreadsList.front();
		m_availableThreadsList.pop_front();
		m_lock.unlock();
		delete thread;
		m_lock.lock();
	}
}


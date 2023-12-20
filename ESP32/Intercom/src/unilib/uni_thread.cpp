#include "uni_thread.h"
#ifdef __POSIX
#include <unistd.h>
#include <limits.h>
#endif



int uni_thread_base::stop()
{
	uni_locker lock(m_lock);
	if (m_state == utsRunning)
	{
		return internalStop();
	}
	else
		return URC_BAD_STATE;
}

void uni_thread_base::onThreadExit()
{
	uni_locker lock(m_lock);
	if (m_state == utsRunning)
	{
		m_state = utsStopped;
	}
	internalOnThreadExit();
}

void uni_thread_base::waitForTermination()
{
	uni_locker lock(m_lock);
	if (m_state == utsRunning)
	{
		internalWaitForTermination();
		if (m_state == utsRunning)
			m_state = utsStopped;
	}
}

int uni_thread_base::start()
{
	uni_locker lock(m_lock);
	if (m_state == utsStopped)
	{
		return internalStart();
	}
	else
		return URC_BAD_STATE;
}

uni_thread_impl::uni_thread_impl(uint32_t stackSize) :
		uni_thread_base(stackSize)
{
	m_thread = 0;
	m_threadID = 0;
}

void uni_thread_impl::internalOnThreadExit()
{
#ifdef __POSIX
	pthread_detach(m_thread);
#endif
	m_thread = 0;
}

#ifdef __POSIX
void uni_thread_impl::internalWaitForTermination()
{
	if (m_thread)
	{
		pthread_t thread = m_thread;
		m_lock.unlock();
		pthread_join(thread, NULL);
		m_lock.lock();
		m_thread = 0;
	}
}

void uni_thread_os::sleep(uint32_t timeout)
{
	usleep(timeout * 1000);
}

int uni_thread_impl::internalStart()
{
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	if (m_stackSize != 0)
		pthread_attr_setstacksize(&attr, m_stackSize<PTHREAD_STACK_MIN?PTHREAD_STACK_MIN:m_stackSize);
	m_state = utsRunning;

	int res = pthread_create(&m_thread, &attr, &run_s, this);
	if (res != 0)
	{
		m_state = utsStopped;
		m_thread = 0;

	}
	pthread_attr_destroy(&attr);

	return m_state == utsRunning ? URC_OK : URC_ERROR;
}
#endif

#ifdef __WINDOWS

uint32_t highTickCount=0;
uint32_t lastTickCount=0;

uint64_t uni_thread_os::getTickCount()
{
	uint32_t tc=GetTickCount();
	if(tc<lastTickCount)
	highTickCount++;
	return (((uint64_t)highTickCount)<<32) | tc;
}

void uni_thread_impl::internalWaitForTermination()
{
	if (m_thread)
	{
		HANDLE thread = m_thread;
		m_lock.unlock();
		WaitForSingleObject(thread,INFINITE);
		m_lock.lock();
		m_thread = 0;
	}
}

void uni_thread_os::sleep(uint32_t timeout)
{
	Sleep(timeout);
}

int uni_thread_impl::internalStart()
{
	m_state = utsRunning;
	m_thread = CreateThread(NULL,m_stackSize,&run_s,this,CREATE_SUSPENDED,NULL);
	if (!m_thread)
	{
		m_state = utsStopped;
	}
	else
	ResumeThread(m_thread);

	return m_state==utsRunning?URC_OK:URC_ERROR;
}
#endif

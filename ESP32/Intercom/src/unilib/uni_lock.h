#ifndef UNI_LOCK_H_
#define UNI_LOCK_H_

#include "uni_common.h"

class uni_lock
{
protected:
public:
	virtual ~uni_lock(){};
	virtual void lock()=0;
	virtual void unlock()=0;
};

class uni_locker
{
protected:
	uni_lock &m_lock;
public:
	uni_locker(uni_lock &lock) :
			m_lock(lock)
	{
		m_lock.lock();
	}

	~uni_locker()
	{
		m_lock.unlock();
	}
};



#endif /* UNI_LOCK_H_ */

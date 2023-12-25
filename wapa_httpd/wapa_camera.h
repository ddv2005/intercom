#ifndef WAPA_CAMERA_H_
#define WAPA_CAMERA_H_
#include <ptlib.h>
#include <ptlib/sockets.h>
#include <list>
#include "wapa_common.h"

#define MAX_CAMERA_BUFFER	512*1024
#define MAX_JPEG_HEADERS_SIZE 128

class PThreadS: public PThread
{
protected:
	PSyncPoint m_exitSignal;
	bool m_exit;
public:

	PThreadS(AutoDeleteFlag deletion = NoAutoDeleteThread,
			Priority priorityLevel = NormalPriority, const PString & threadName =
					PString::Empty()) :
			PThread(5000, deletion, priorityLevel, threadName)
	{
		m_exit = false;
	}

	void Stop()
	{
		m_exit = true;
		m_exitSignal.Signal();
	}

	PSyncPoint &GetExitSignal()
	{
		return m_exitSignal;
	}

	bool IsStop()
	{
		return m_exit;
	}
};

template<typename T>
class PThreadFunction: public PThreadS
{
	typedef void (T::*thread_func)(PThreadFunction<T> *thread);
protected:
	thread_func m_func;
	T &m_owner;
public:

	PThreadFunction(T &owner, thread_func func, AutoDeleteFlag deletion =
			NoAutoDeleteThread, Priority priorityLevel = NormalPriority,
			const PString & threadName = PString::Empty()) :
			PThreadS(deletion, priorityLevel, threadName), m_func(func), m_owner(
					owner)
	{
	}

	virtual void Main()
	{
		(m_owner.*m_func)(this);
	}
};

class WAPAIPCamera;
class WAPACamera;

#define PCameraThread PThreadFunction<WAPAIPCamera>

class WAPACameraCallback
{
public:
	virtual void OnVideoFrame(WAPACamera *camera, unsigned char type,
			unsigned int width, unsigned int height, unsigned char *data, int size,
			int interval)=0;
	virtual ~WAPACameraCallback(){};
};

class WAPACamera
{
protected:
	WAPACameraCallback *m_callback;
public:
	WAPACamera(WAPACameraCallback *callback) :
			m_callback(callback)
	{
	}
	;
	virtual int Start()=0;
	virtual void Stop()=0;
	virtual ~WAPACamera()
	{
	}
	;
};

class WAPAIPCamera: public WAPACamera
{
	friend class PCameraThread;
protected:
	PIPSocket::Address m_address;
	WORD m_port;
	PCameraThread *m_thread;
	extra_camera_params_t m_extra_params;

	void CameraThread(PCameraThread *thread);
public:
	WAPAIPCamera(WAPACameraCallback *callback,PIPSocket::Address address, WORD port,extra_camera_params_t &extra_params);
	virtual int Start();
	virtual void Stop();
	virtual ~WAPAIPCamera();
};

#endif /* WAPA_CAMERA_H_ */

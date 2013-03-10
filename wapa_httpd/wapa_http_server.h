#ifndef WAPA_HTTP_SERVER_H
#define WAPA_HTTP_SERVER_H
#include <ptlib.h>
#include <ptclib/http.h>
#include <ptlib/sockets.h>
#include <list>
#include "wapa_common.h"
#include "wapa_camera.h"

using namespace std;

#define WAPA_MPEG4_HD_URL 	"hd"
#define WAPA_MPEG4_CIF_URL 	"cif"
#define WAPA_MJPEG_URL 		"mjpg"
#define WAPA_JPEG_URL 		"jpg"
#define WAPA_SND_MULAW_URL 		"mulaw"
#define CAMERA_STOP_THRESHOLD_TIME	20000

class WAPACameraParams
{
public:
	PString m_name;
	PString m_camera_address;
	PString m_device;
	PString m_device_params;
	WORD m_camera_port;
	extra_camera_params_t m_extra;

	WAPACameraParams()
	{
		m_extra.m_type = CT_MPEG4_WAPA;
		m_camera_port = 0;
	}
};

typedef list<WAPACameraParams*> CameraParams;
typedef list<WAPACameraParams*>::iterator CameraParamsItr;

class WAPAHTTPServerParams
{
public:
	PString ip;
	PString mac;
	WORD port;
	PString user_name;
	PString password;
	CameraParams cameras;

	WAPAHTTPServerParams()
	{
		ip = "0.0.0.0";
		port = 8080;
		user_name = "root";
		password = "wapa";
		mac = "00:10:00:00:00:00";
	}

	void ClearCameras()
	{
		for (CameraParamsItr itr = cameras.begin(); itr != cameras.end(); itr++)
			delete (*itr);
	}

	~WAPAHTTPServerParams()
	{
		ClearCameras();
	}
};

class WAPAHTTPServer;
class HTTPMPEG4Resource;
class WAPAHTTPCamera;

class PHTTPServiceThread: public PThread
{
public:
	PHTTPServiceThread(PINDEX stackSize, WAPAHTTPServer & app);
	virtual ~PHTTPServiceThread();

	void Main();
	void Close();

protected:
	PINDEX myStackSize;
	WAPAHTTPServer & process;
	PTCPSocket * socket;
};

struct FrameMemoryHolder
{
	unsigned int size;
	void * pointer;
};

class FrameMemoryAllocator
{
protected:
	PMutex m_lock;
	unsigned int m_min_size;
	unsigned int m_max_count;
	unsigned char m_inc_bits;
	FrameMemoryHolder *m_array;
	unsigned int m_free;
	unsigned int m_used;
	unsigned int m_miss_cnt;

	void Reset()
	{
		for (unsigned int i = 0; i < m_max_count; i++)
		{
			if (m_array[i].size > 0)
			{
				m_array[i].size = 0;
				free(m_array[i].pointer);
			}
		}
		m_free = m_max_count;
		m_used = 0;
		m_miss_cnt = 0;
		PTRACE(3, "Reset FrameMemoryAllocator");
	}

public:
	FrameMemoryAllocator(unsigned int min_size, unsigned int max_count,
			unsigned char inc_bits = 15)
	{
		m_inc_bits = inc_bits;
		m_min_size = min_size;
		m_free = m_max_count = max_count;
		m_used = m_miss_cnt = 0;
		m_array = (FrameMemoryHolder*) calloc(max_count, sizeof(FrameMemoryHolder));
	}

	void *Alloc(unsigned int size, unsigned int &allocated_size)
	{
		void * result = NULL;
		if (size >= m_min_size)
		{
			PWaitAndSignal lock(m_lock);
			allocated_size = 0;
			unsigned int idx = 0;
			unsigned int used = m_used;
			unsigned int projected_allocated_size = ((size >> m_inc_bits) + 1)
					<< m_inc_bits;
			for (unsigned int i = 0; (i < m_max_count) && (used > 0); i++)
			{
				if (m_array[i].size > 0)
				{
					used--;
					if (m_array[i].size >= size)
					{
						if ((allocated_size == 0) || (m_array[i].size < allocated_size))
						{
							allocated_size = m_array[i].size;
							result = m_array[i].pointer;
							idx = i;
							if (allocated_size == projected_allocated_size)
								break;
						}
					}
				}
			}
			if (result)
			{
				m_used--;
				m_free++;
				m_array[idx].size = 0;
				m_miss_cnt = 0;
			}
			else
			{
				if (m_free == 0)
					m_miss_cnt++;
				if (m_miss_cnt > 10)
					Reset();
				allocated_size = projected_allocated_size;
				result = malloc(allocated_size);
			}
		}
		else
		{
			allocated_size = size;
			result = malloc(size);
		}
		return result;
	}

	void Free(void *pointer, unsigned int allocated_size)
	{
		if (allocated_size >= m_min_size)
		{
			PWaitAndSignal lock(m_lock);
			if (m_free > 0)
			{
				for (unsigned int i = 0; i < m_max_count; i++)
				{
					if (m_array[i].size == 0)
					{
						m_free--;
						m_used++;
						m_array[i].size = allocated_size;
						m_array[i].pointer = pointer;
						pointer = NULL;
						break;
					}
				}
			}
			if (pointer)
			{
				free(pointer);
			}
		}
		else
		{
			free(pointer);
		}
	}

	~FrameMemoryAllocator()
	{
		Reset();
		delete m_array;
	}
};

class FrameHolder
{
public:
	static FrameMemoryAllocator m_allocator;
	static PMutex m_counter_lock;
	int m_counter;
	unsigned char * m_frame;
	unsigned int m_size;
	unsigned int m_alloc_size;
	unsigned char m_ftype;
	unsigned int m_width, m_height;
	FrameHolder(unsigned char * frame, unsigned int size, unsigned char ftype,
			unsigned int width, unsigned int height);
	~FrameHolder()
	{
		if (m_frame)
		{
			m_allocator.Free(m_frame, m_alloc_size);
		}
	}

	FrameHolder* AddRef()
	{
		PWaitAndSignal lock(m_counter_lock);
		m_counter++;
		return this;
	}

	bool Release()
	{
		PWaitAndSignal lock(m_counter_lock);
		return (--m_counter) > 0;
	}

	void ReleaseAndDelete()
	{
		if (!Release())
			delete this;
	}
};

typedef list<FrameHolder*> FrameList;
typedef list<FrameHolder*>::iterator FrameListItr;

#define PJS_FLOAT_SHIFT	16
#define PJS_FLOAT_CONST32(c,bits) ((int) ((c) *((((int)1)<< bits))))
#define PJS_FLOAT_EXT_CONST(c) PJS_FLOAT_CONST32(c,PJS_FLOAT_SHIFT)
#define PJS_FLOAT_ONE_CONST (((int)1)<<PJS_FLOAT_SHIFT)

#define MAX_FRAME_CONST PJS_FLOAT_EXT_CONST(100)

class RequestHolder
{
protected:
	unsigned char m_type;
	bool m_first;
	bool m_iframe;
	bool m_exit;
	PMutex m_lock;
	PSyncPoint m_syncpoint;
	FrameList m_frames;
	WAPAHTTPCamera *m_camera;
	int m_interval;
	PInt64 m_last_frame_time;
	int m_frame_current;
	int m_frame_inc;
public:
	RequestHolder(unsigned char type, WAPAHTTPCamera *camera, unsigned interval =
			0) :
			m_type(type), m_camera(camera), m_interval(interval)
	{
		m_frame_current = m_frame_inc = 0;
		if (m_interval >= 1)
			m_interval--;
		m_iframe = m_first = true;
		m_exit = false;
		m_last_frame_time = 0;
	}

	~RequestHolder()
	{
		for (FrameListItr itr = m_frames.begin(); itr != m_frames.end(); itr++)
			DeleteFrame(*itr);
	}

	bool checkType(unsigned char type)
	{
		return (type & 0xF) == m_type;
	}

	bool checkCamera(WAPAHTTPCamera *camera)
	{
		return camera == m_camera;
	}

	void Wait(PTimeInterval timeout)
	{
		m_syncpoint.Wait(timeout);
	}

	FrameHolder* GetFrameAndRemove()
	{
		FrameHolder* result = NULL;
		PWaitAndSignal lock(m_lock);
		if (m_frames.size() > 0)
		{
			result = *m_frames.begin();
			m_frames.pop_front();
		}
		return result;
	}

	void DeleteFrame(FrameHolder* frame)
	{
		frame->ReleaseAndDelete();
	}

	bool AddFrame(FrameHolder *frame, int interval)
	{
		bool result = true;
		PWaitAndSignal lock(m_lock);
		if (m_interval > 0)
		{
			if (interval)
			{
				if (interval < m_interval)
				{
					m_frame_inc = PJS_FLOAT_EXT_CONST(interval) / m_interval;
				}
				else
					m_frame_current = m_frame_inc = 0;

			}
			else
			{
				m_frame_current = m_frame_inc = 0;
			}
			if (m_frame_inc)
			{
				if (m_frame_current)
				{
					if (m_frame_current > MAX_FRAME_CONST )
						m_frame_current -= MAX_FRAME_CONST;
					int last = m_frame_current >> PJS_FLOAT_SHIFT;
					m_frame_current += m_frame_inc;
					if (last == (m_frame_current >> PJS_FLOAT_SHIFT))
					{
						return false;
					}
				}
				else
					m_frame_current += m_frame_inc;
			}
		}
		if (m_frames.size() <= 6)
		{

			m_frames.push_back(frame->AddRef());
			m_syncpoint.Signal();
		}
		else
		{
			m_iframe = true;
			result = false;
		}
		return result;
	}

	bool needIFrame()
	{
		return m_iframe;
	}
	void clearIFrame()
	{
		m_iframe = false;
	}
	bool isFirst()
	{
		return m_first;
	}
	void clearFirst()
	{
		m_first = false;
	}
	bool isExit()
	{
		return m_exit;
	}
	void setExit()
	{
		m_exit = true;
		m_syncpoint.Signal();
	}

};

typedef list<RequestHolder*> RequestList;
typedef list<RequestHolder*>::iterator RequestListItr;
typedef list<PHTTPServiceThread*> ThreadList;

class WAPAHTTPCamera: public WAPACameraCallback
{
protected:
	PString m_name;
	PMutex m_camera_mutex;
	int m_camera_usage;
	WAPACamera *m_camera;
	PTime m_camera_stop_time;
	WAPAHTTPServer &m_server;
	WAPACameraParams *m_parameters;

	virtual void OnVideoFrame(WAPACamera *camera, unsigned char type,
			unsigned int width, unsigned int height, unsigned char *data, int size,
			int interval);
public:
	WAPAHTTPCamera(WAPAHTTPServer &server, PString &name, WAPACameraParams *params) :
			m_name(name), m_server(server), m_parameters(params)
	{
		m_camera_usage = 0;
		m_camera = NULL;
	}
	virtual ~WAPAHTTPCamera()
	{
		InternalStopCamera();
	}
	void InternalStopCamera();
	void StartCamera();
	void StopCamera();
	void Tick();
};

typedef list<WAPAHTTPCamera*> HTTPCamerasList;
typedef list<WAPAHTTPCamera*>::iterator HTTPCamerasListItr;

class WAPAHTTPServer
{
	friend class PHTTPServiceThread;
	friend class HTTPMPEG4Resource;
	friend class WAPAHTTPCamera;
	friend class HTTPMJPEGResource;
	friend class HTTPJPEGResource;
protected:
	PIPSocket * m_http_listening_socket;
	WAPAHTTPServerParams m_params;
	ThreadList m_http_threads;
	PMutex m_http_threads_mutex;
	PHTTPSpace m_http_name_space;
	RequestList m_requests;
	PMutex m_requests_mutex;
	HTTPCamerasList m_cameras;

	void ShutdownListener();

	PBoolean ListenForHTTP(PString &address, WORD port,
			PSocket::Reusability reuse, PINDEX stackSize);
	PBoolean ListenForHTTP(PString &address, PIPSocket * listener,
			PSocket::Reusability reuse, PINDEX stackSize);
	PHTTPServer * CreateHTTPServer(PTCPSocket & socket);
	PTCPSocket * AcceptHTTP();
	PBoolean ProcessHTTP(PTCPSocket & socket);
	void OnVideoFrame(WAPAHTTPCamera *camera, unsigned char type,
			unsigned int width, unsigned int height, unsigned char *data, int size,
			int interval);
	int LoadConfig(PString &config_file);
	void StopAllCameras();
public:
	WAPAHTTPServer(PString &config_file);
	virtual ~WAPAHTTPServer();
	WAPAHTTPServerParams &GetParams()
	{
		return m_params;
	}
	void AddRequest(RequestHolder *rh);
	void DelRequest(RequestHolder *rh);
	int Init();
	void Deinit();
	void Tick();
};

class HTTPMPEG4Resource: public PHTTPResource
{
protected:
	unsigned char m_ftype;
	WAPAHTTPServer &m_server;
	WAPAHTTPCamera *m_camera;
public:
	HTTPMPEG4Resource(WAPAHTTPServer &server, WAPAHTTPCamera *camera,
			const PURL & url, ///< Name of the resource in URL space.
			const PHTTPAuthority & auth, ///< Authorisation for the resource.
			unsigned char ftype);
	virtual ~HTTPMPEG4Resource();
	virtual PBoolean LoadHeaders(PHTTPRequest & request ///<  Information on this request.
			);
	virtual void SendData(PHTTPRequest & request ///< information for this request
			);
	virtual PBoolean GetExpirationDate(PTime & when ///< Time that the resource expires
			);
};

class HTTPMJPEGResource: public PHTTPResource
{
protected:
	unsigned char m_ftype;
	WAPAHTTPServer &m_server;
	WAPAHTTPCamera *m_camera;
	PString m_boundary;
public:
	HTTPMJPEGResource(WAPAHTTPServer &server, WAPAHTTPCamera *camera,
			const PURL & url, ///< Name of the resource in URL space.
			const PHTTPAuthority & auth, ///< Authorisation for the resource.
			unsigned char ftype);
	virtual ~HTTPMJPEGResource();
	virtual PBoolean LoadHeaders(PHTTPRequest & request ///<  Information on this request.
			);
	virtual void SendData(PHTTPRequest & request ///< information for this request
			);
	virtual PBoolean GetExpirationDate(PTime & when ///< Time that the resource expires
			);
};

class HTTPJPEGResource: public PHTTPResource
{
protected:
	unsigned char m_ftype;
	WAPAHTTPServer &m_server;
	WAPAHTTPCamera *m_camera;
public:
	HTTPJPEGResource(WAPAHTTPServer &server, WAPAHTTPCamera *camera,
			const PURL & url, ///< Name of the resource in URL space.
			const PHTTPAuthority & auth, ///< Authorisation for the resource.
			unsigned char ftype);
	virtual ~HTTPJPEGResource();
	virtual PBoolean LoadHeaders(PHTTPRequest & request ///<  Information on this request.
			);
	virtual void SendData(PHTTPRequest & request ///< information for this request
			);
	virtual PBoolean GetExpirationDate(PTime & when ///< Time that the resource expires
			);
};

class HTTPStreamingResource: public PHTTPResource
{
protected:
	unsigned char m_ftype;
	WAPAHTTPServer &m_server;
	WAPAHTTPCamera *m_camera;
public:
	HTTPStreamingResource(WAPAHTTPServer &server, WAPAHTTPCamera *camera,
			const PURL & url, ///< Name of the resource in URL space.
			const PHTTPAuthority & auth, ///< Authorisation for the resource.
			unsigned char ftype,PString content_type);
	virtual ~HTTPStreamingResource();
	virtual PBoolean LoadHeaders(PHTTPRequest & request ///<  Information on this request.
			);
	virtual void SendData(PHTTPRequest & request ///< information for this request
			);
	virtual PBoolean GetExpirationDate(PTime & when ///< Time that the resource expires
			);
};


class HTTPMSoundBasicResource: public PHTTPResource
{
protected:
	unsigned char m_ftype;
	WAPAHTTPServer &m_server;
	WAPAHTTPCamera *m_camera;
public:
	HTTPMSoundBasicResource(WAPAHTTPServer &server, WAPAHTTPCamera *camera,
			const PURL & url, ///< Name of the resource in URL space.
			const PHTTPAuthority & auth, ///< Authorisation for the resource.
			unsigned char ftype);
	virtual ~HTTPMSoundBasicResource();
	virtual PBoolean LoadHeaders(PHTTPRequest & request ///<  Information on this request.
			);
	virtual void SendData(PHTTPRequest & request ///< information for this request
			);
	virtual PBoolean GetExpirationDate(PTime & when ///< Time that the resource expires
			);
};


#endif //WAPA_HTTP_SERVER_H

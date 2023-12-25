#include "wapa_http_server.h"
#include "wapa_uvc_camera.h"
#include "wapa_zmq_sound.h"
#include <libconfig.h++>

#define WAPA_C_MAIN		"WAPA_HTTPD"
#define WAPA_C_CAMERAS	"CAMERAS"

unsigned char jpeg_default_ht[] =
		{ 0xff, 0xd8, 0xff, 0xc4, 0x00, 0x1f, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01,
				0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0xff,
				0xc4, 0x00, 0xb5, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03,
				0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7d, 0x01, 0x02, 0x03, 0x00,
				0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
				0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1,
				0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
				0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x34, 0x35,
				0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
				0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65,
				0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
				0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94,
				0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
				0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba,
				0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
				0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6,
				0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
				0xf9, 0xfa, 0xff, 0xc4, 0x00, 0x1f, 0x01, 0x00, 0x03, 0x01, 0x01, 0x01,
				0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0xff,
				0xc4, 0x00, 0xb5, 0x11, 0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04,
				0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77, 0x00, 0x01, 0x02, 0x03,
				0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
				0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09,
				0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
				0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a,
				0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
				0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64,
				0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
				0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92,
				0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
				0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8,
				0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
				0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe2, 0xe3, 0xe4, 0xe5,
				0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
				0xf9, 0xfa };

int is_huffman(unsigned char *buf)
{
	unsigned char *ptbuf;
	int i = 0;
	ptbuf = buf;
	while (((ptbuf[0] << 8) | ptbuf[1]) != 0xffda)
	{
		if (i++ > 2048)
			return 0;
		if (((ptbuf[0] << 8) | ptbuf[1]) == 0xffc4)
			return 1;
		ptbuf++;
	}
	return 0;
}


FrameHolder::FrameHolder(unsigned char * frame, unsigned int size,
		unsigned char ftype, unsigned int width, unsigned int height) :
		m_counter(1)
{
	unsigned int new_size = size;
	m_alloc_size = 0;
	if ((ftype == MPEG4_HD_TYPE) || (ftype == MPEG4_CIF_TYPE))
	{
		unsigned char vol[9] =
		{ 0x00, 0x00, 0x01, 0xE0, 0x00, 0x00, 0x80, 0x00, 0x00 };
		unsigned int new_size = size + sizeof(vol);
		vol[4] = ((size + 3) >> 8);
		vol[5] = size + 3;

		m_frame = (unsigned char*) m_allocator.Alloc(new_size, m_alloc_size);
		memcpy(m_frame, vol, sizeof(vol));
		memcpy(&m_frame[sizeof(vol)], frame, size);
		m_size = new_size;
	}
	else if ((ftype == JPEG_TYPE) && (size > 2))
	{
		if(!is_huffman(frame))
		{
			new_size = size + sizeof(jpeg_default_ht) - 2;
			m_frame = (unsigned char*) m_allocator.Alloc(new_size, m_alloc_size);
			memcpy(m_frame, jpeg_default_ht, sizeof(jpeg_default_ht));
			memcpy(&m_frame[sizeof(jpeg_default_ht)], &frame[2], size - 2);
		}
		else
		{
			m_frame = (unsigned char*) m_allocator.Alloc(new_size, m_alloc_size);
			memcpy(m_frame, frame, size);
		}
	}
	else
	{
		m_frame = (unsigned char*) m_allocator.Alloc(new_size, m_alloc_size);
		memcpy(m_frame, frame, size);
	}
	m_size = new_size;
	m_ftype = ftype;
	m_width = width;
	m_height = height;
}

FrameMemoryAllocator FrameHolder::m_allocator(1024, 32, 15);
PMutex FrameHolder::m_counter_lock;
static const PTime ImmediateExpiryTime(0, 0, 0, 1, 1, 1980);
using namespace libconfig;
WAPAHTTPServer::WAPAHTTPServer(PString &config_file) :
		m_params()
{
	m_http_listening_socket = NULL;
	LoadConfig(config_file);
}

int WAPAHTTPServer::LoadConfig(PString &config_file)
{
	int result = -1;
	libconfig::Config config;
	FILE *f = NULL;
#if defined(__WINDOWS)
	fopen_s(&f,(const char *)config_file, "rt");
#else
	f = fopen((const char *) config_file, "rt");
#endif
	if (f == NULL)
	{
		PTRACE(1, "Can't open config file "<<config_file);
	}
	else
	{
		try
		{
			try
			{
				try
				{
					try
					{
						config.read(f);
						fclose(f);

						if (config.exists(WAPA_C_MAIN))
						{
							Setting &main = config.lookup(WAPA_C_MAIN);
							unsigned int ui;
							std::string str;
							if (main.lookupValue("MAC", str))
							{
								m_params.mac = str.c_str();
							}
							if (main.lookupValue("BIND_ADDRESS", str))
							{
								m_params.ip = str.c_str();
							}
							if (main.lookupValue("USER_NAME", str))
							{
								m_params.user_name = str.c_str();
							}
							if (main.lookupValue("PASSWORD", str))
							{
								m_params.password = str.c_str();
							}
							if (main.lookupValue("PORT", ui))
							{
								m_params.port = (WORD) ui;
							}
							m_params.ClearCameras();
							if (main.exists(WAPA_C_CAMERAS))
							{
								Setting &cameras = main[WAPA_C_CAMERAS];
								if (cameras.getType() == Setting::TypeGroup)
								{
									for (int i = 0; i < cameras.getLength(); i++)
									{
										Setting & cameraSettings = cameras[i];
										if (cameraSettings.getType() == Setting::TypeGroup)
										{
											WAPACameraParams *wapa_camera = new WAPACameraParams();
											wapa_camera->m_name = cameraSettings.getName();
											if (cameraSettings.lookupValue("TYPE", ui))
											{
												wapa_camera->m_extra.m_type = (BYTE) ui;
											}

											if (cameraSettings.lookupValue("DEVICE", str))
											{
												wapa_camera->m_device = str.c_str();
											}

											if (cameraSettings.lookupValue("DEVICE_PARAMS", str))
											{
												wapa_camera->m_device_params = str.c_str();
											}

											if (cameraSettings.lookupValue("ADDRESS", str))
											{
												wapa_camera->m_camera_address = str.c_str();
												if (cameraSettings.lookupValue("PORT", ui))
												{
													wapa_camera->m_camera_port = (WORD) ui;
												}
											}
											PTRACE(3,
													"Add camera "<<wapa_camera->m_name<<" ("<<wapa_camera->m_camera_address<<":"<<wapa_camera->m_camera_port<<"); type="<<(int)wapa_camera->m_extra.m_type<<"; device="<<wapa_camera->m_device<<"; device_params="<<wapa_camera->m_device_params);
											m_params.cameras.push_back(wapa_camera);
										}
									}
								}
							}

							result = 0;
						}
					} catch (...)
					{
						fclose(f);
						throw;
					}
				} catch (libconfig::ParseException &ee)
				{
					PString tmp;
					tmp.sprintf("LoadConfig error at line %d (%s)", ee.getLine(),
							ee.getError());
					PTRACE(1, tmp);
				}
			} catch (libconfig::SettingException &s)
			{
				PString tmp;
				tmp.sprintf("LoadConfig %s, path: %s", s.what(), s.getPath());
				PTRACE(1, tmp);
			}
		} catch (std::exception &e)
		{
			PString tmp;
			tmp.sprintf("LoadConfig unhandled exception (%s)", e.what());
			PTRACE(1, tmp);
		}
	}
	return result;
}

WAPAHTTPServer::~WAPAHTTPServer()
{
	Deinit();
}

int WAPAHTTPServer::Init()
{
	int result = WR_ERROR;
	if (!m_http_listening_socket)
	{
		if (ListenForHTTP(m_params.ip, m_params.port, PSocket::CanReuseAddress,
				0x4000) == PTrue)
		{
			m_http_name_space.AddResource(
					new PHTTPString("execute.cgi?opname=get_unit_info",
							"board_mac: " + m_params.mac));
			PHTTPSimpleAuth auth("WAPA", m_params.user_name, m_params.password);
			for (CameraParamsItr itr = m_params.cameras.begin();
					itr != m_params.cameras.end(); itr++)
			{
				WAPAHTTPCamera *http_camera = new WAPAHTTPCamera(*this, (*itr)->m_name,
						*itr);
				m_cameras.push_back(http_camera);
				m_http_name_space.AddResource(
						new HTTPMPEG4Resource(*this, http_camera,
								(*itr)->m_name + "/" + WAPA_MPEG4_HD_URL, auth, MPEG4_HD_TYPE));
				m_http_name_space.AddResource(
						new HTTPMPEG4Resource(*this, http_camera,
								(*itr)->m_name + "/" + WAPA_MPEG4_CIF_URL, auth,
								MPEG4_CIF_TYPE));
				m_http_name_space.AddResource(
						new HTTPMJPEGResource(*this, http_camera,
								(*itr)->m_name + "/" + WAPA_MJPEG_URL, auth, JPEG_TYPE));
				m_http_name_space.AddResource(
						new HTTPJPEGResource(*this, http_camera,
								(*itr)->m_name + "/" + WAPA_JPEG_URL, auth, JPEG_TYPE));
				m_http_name_space.AddResource(
						new HTTPStreamingResource(*this, http_camera,
								(*itr)->m_name + "/" + WAPA_SND_MULAW_URL, auth, SND_MULAW_TYPE,"audio/basic"));
			}
			result = WR_OK;
		}
		else
		{
		    PTRACE(1,"Can't listen");
		}
	}
	return result;
}

PBoolean WAPAHTTPServer::ListenForHTTP(PString &address, WORD port,
		PSocket::Reusability reuse, PINDEX stackSize)
{
	if (m_http_listening_socket != NULL
			&& m_http_listening_socket->GetPort() == port
			&& m_http_listening_socket->IsOpen())
		return PTrue;

	return ListenForHTTP(address, new PTCPSocket(port), reuse, stackSize);
}

PBoolean WAPAHTTPServer::ListenForHTTP(PString &address, PIPSocket * listener,
		PSocket::Reusability reuse, PINDEX stackSize)
{
	if (m_http_listening_socket != NULL)
		ShutdownListener();

	m_http_listening_socket = PAssertNULL(listener);
	PIPSocket::Address _address(address);
	if (!m_http_listening_socket->Listen(_address, 5, 0, reuse))
	{
		PTRACE( DEBUG_LEVEL,
				"HTTP\tListen on port " << m_http_listening_socket->GetPort() << " failed: " << m_http_listening_socket->GetErrorText());
		return PFalse;
	}

	if (stackSize > 1000)
		new PHTTPServiceThread(stackSize, *this);

	return PTrue;
}

PTCPSocket * WAPAHTTPServer::AcceptHTTP()
{
	if (m_http_listening_socket == NULL)
		return NULL;

	if (!m_http_listening_socket->IsOpen())
		return NULL;

	// get a socket when a client connects
	PTCPSocket * socket = new PTCPSocket;
	if (socket->Accept(*m_http_listening_socket))
		return socket;

	if (socket->GetErrorCode() != PChannel::Interrupted)
	{
		PTRACE(ERROR_LEVEL, "Accept failed for HTTP: " << socket->GetErrorText());
	}

	if (m_http_listening_socket != NULL && m_http_listening_socket->IsOpen())
		return socket;

	delete socket;
	return NULL;
}

PBoolean WAPAHTTPServer::ProcessHTTP(PTCPSocket & socket)
{
	if (!socket.IsOpen())
		return PTrue;

	PHTTPServer * server = CreateHTTPServer(socket);
	if (server == NULL)
	{
		PTRACE(ERROR_LEVEL, "HTTP server creation/open failed.");
		return PTrue;
	}

	// process requests
	while (server->ProcessCommand())
		;

	// always close after the response has been sent
	delete server;

	return PTrue;
}

void WAPAHTTPServer::Deinit()
{
	StopAllCameras();
	ShutdownListener();
}

void WAPAHTTPServer::StopAllCameras()
{
	for (HTTPCamerasListItr itr = m_cameras.begin(); itr != m_cameras.end();
			itr++)
		(*itr)->InternalStopCamera();
}

void WAPAHTTPServer::ShutdownListener()
{
	if (m_http_listening_socket == NULL)
		return;

	if (!m_http_listening_socket->IsOpen())
	{
		delete m_http_listening_socket;
		m_http_listening_socket = NULL;
		return;
	}

	PTRACE( DEBUG_LEVEL,
			"HTTPSVC\tClosing listener socket on port " << m_http_listening_socket->GetPort());

	m_http_listening_socket->Close();

	m_requests_mutex.Wait();
	for (RequestListItr itr = m_requests.begin(); itr != m_requests.end(); itr++)
	{
		(*itr)->setExit();
	}
	m_requests.clear();
	m_requests_mutex.Signal();

	m_http_threads_mutex.Wait();
	for (ThreadList::iterator i = m_http_threads.begin();
			i != m_http_threads.end(); i++)
		(*i)->Close();

	while (m_http_threads.size() > 0)
	{
		m_http_threads_mutex.Signal();
		PThread::Sleep(1);
		m_http_threads_mutex.Wait();
	}

	m_http_threads_mutex.Signal();

	delete m_http_listening_socket;
	m_http_listening_socket = NULL;
}

PHTTPServer * WAPAHTTPServer::CreateHTTPServer(PTCPSocket & socket)
{
#ifdef SO_LINGER
	const linger ling =
	{ 1, 5 };
	socket.SetOption(SO_LINGER, &ling, sizeof(ling));
#endif

	PHTTPServer * server = new PHTTPServer(m_http_name_space);

	if (server->Open(socket))
		return server;

	delete server;
	return NULL;
}

void WAPAHTTPServer::Tick()
{
	for (HTTPCamerasListItr itr = m_cameras.begin(); itr != m_cameras.end();
			itr++)
		(*itr)->Tick();
}

void WAPAHTTPServer::OnVideoFrame(WAPAHTTPCamera *camera, unsigned char type,
		unsigned int width, unsigned int height, unsigned char *data, int size,
		int interval)
{
	FrameHolder *frame = new FrameHolder(data, size, type, width, height);
	PWaitAndSignal lock(m_requests_mutex);
	for (RequestListItr itr = m_requests.begin(); itr != m_requests.end(); itr++)
	{
		if ((*itr)->checkCamera(camera))
		{
			if ((*itr)->checkType(type))
			{
				if ((type == MPEG4_HD_TYPE) || (type == MPEG4_CIF_TYPE))
				{
					bool canContinue = true;
					bool isIFrame = !(type & 0x80);
					if ((*itr)->isFirst())
					{
						if (isIFrame)
						{
							BYTE vol[27] =
							{ 0x00, 0x00, 0x01, 0xB0, 0xF5, 0x00, 0x00, 0x01, 0xB5, 0x00,
									0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x20, 0x00, 0x84,
									0x40, 0x07, 0x00, 0x00, 0x00, 0x00, 0xa2 };

							unsigned int ui = (height | (1 << 13) | (width << 14)
									| ((unsigned int) 21 << 27));

							vol[25] = (unsigned char) ui;
							vol[24] = (unsigned char) (ui >> 8);
							vol[23] = (unsigned char) (ui >> 16);
							vol[22] = (unsigned char) (ui >> 24);
							FrameHolder *fframe = new FrameHolder(vol, sizeof(vol), 0xFF,
									width, height);
							if ((*itr)->AddFrame(fframe, interval))
							{
								(*itr)->clearFirst();
								(*itr)->clearIFrame();
							}
							else
								canContinue = false;
							fframe->ReleaseAndDelete();
						}
					}
					if (canContinue)
					{
						if ((*itr)->needIFrame())
						{
							if (isIFrame)
							{
								if ((*itr)->AddFrame(frame, interval))
									(*itr)->clearIFrame();
							}
						}
						else
							(*itr)->AddFrame(frame, interval);
					}
				}
				else
				{
					(*itr)->AddFrame(frame, interval);
				}
			}
		}
	}
	frame->ReleaseAndDelete();
}

void WAPAHTTPServer::AddRequest(RequestHolder *rh)
{
	PWaitAndSignal lock(m_requests_mutex);
	m_requests.push_back(rh);
}

void WAPAHTTPServer::DelRequest(RequestHolder *rh)
{
	PWaitAndSignal lock(m_requests_mutex);
	m_requests.remove(rh);
}

//=====================================================

PHTTPServiceThread::PHTTPServiceThread(PINDEX stackSize, WAPAHTTPServer & app) :
		PThread(stackSize, AutoDeleteThread, NormalPriority, "WAPA HTTP Service"), process(
				app)
{
	process.m_http_threads_mutex.Wait();
	process.m_http_threads.push_back(this);
	process.m_http_threads_mutex.Signal();

	myStackSize = stackSize;
	socket = NULL;
	Resume();
}

PHTTPServiceThread::~PHTTPServiceThread()
{
	process.m_http_threads_mutex.Wait();
	process.m_http_threads.remove(this);
	process.m_http_threads_mutex.Signal();
	delete socket;
}

void PHTTPServiceThread::Close()
{
	if (socket != NULL)
		socket->Close();
}

void PHTTPServiceThread::Main()
{
	socket = process.AcceptHTTP();
	if (socket != NULL)
	{
		new PHTTPServiceThread(myStackSize, process);
		process.ProcessHTTP(*socket);
	}
}

//=====================================================
HTTPMPEG4Resource::HTTPMPEG4Resource(WAPAHTTPServer &server,
		WAPAHTTPCamera *camera, const PURL & url, ///< Name of the resource in URL space.
		const PHTTPAuthority & auth, ///< Authorisation for the resource.
		unsigned char ftype) :
		PHTTPResource(url, auth), m_server(server), m_camera(camera)
{
	m_ftype = ftype;
	contentType = "video/mpeg4-generic";
}

PBoolean HTTPMPEG4Resource::LoadHeaders(PHTTPRequest & request)
{
	request.contentSize = P_MAX_INDEX;
	return PTrue;
}

HTTPMPEG4Resource::~HTTPMPEG4Resource()
{
}

void HTTPMPEG4Resource::SendData(PHTTPRequest & request ///< information for this request
		)
{
	request.outMIME.SetAt(PHTTP::ContentTypeTag(), contentType);
	request.outMIME.SetAt(PHTTP::ConnectionTag(), "Close");
	request.outMIME.RemoveAt(PHTTP::ContentLengthTag());
	request.outMIME.RemoveAt(PHTTP::DateTag());
	request.outMIME.RemoveAt(PHTTP::ExpiresTag());

	PCharArray data;
	//  request.server.StartResponse(request.code, request.outMIME, 0);

	request.server << "HTTP/1.0 200 OK\r\n";
	request.server << setfill('\r') << request.outMIME;

	m_camera->StartCamera();

	try
	{
		request.server.flush();
		int socket_handle = request.server.GetSocket()->GetHandle();
		RequestHolder *rh = new RequestHolder(m_ftype, m_camera);
		m_server.AddRequest(rh);
		PTRACE(3, "Start Request");

		bool wait = false;
		timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 200000;
		int no_frame_cnt = 0;
		FrameHolder *frame = NULL;
		unsigned int frame_position = 0;

		while (!rh->isExit())
		{
			if (!frame)
				frame = rh->GetFrameAndRemove();
			if (frame)
			{
				no_frame_cnt = 0;
				int retval;
				if (wait)
				{
					fd_set writefds;
					FD_ZERO(&writefds);
					FD_SET(socket_handle, &writefds);

					tv.tv_sec = 0;
					tv.tv_usec = 200000;

					retval = ::select(socket_handle + 1, NULL, &writefds, NULL, &tv);
				}
				else
					retval = 1;
				if (retval > 0)
				{
					if (frame_position < frame->m_size)
						retval = ::send(socket_handle,
								(char*) &frame->m_frame[frame_position],
								frame->m_size - frame_position, 0);
					else
						retval = 0;
					if ((retval > 0) || (frame_position >= frame->m_size))
					{
						wait = false;
						frame_position += retval;
						if (frame_position >= frame->m_size)
						{
							rh->DeleteFrame(frame);
							frame_position = 0;
							frame = NULL;
						}
					}
					else
					{
						if (retval == 0)
							wait = true;
						else
						{
							int e = errno;
							if (e != EWOULDBLOCK)
							{
								PTRACE(2, "Socket write error "<<e);
								break;
							}
							else
								wait = true;
						}
					}

				}
			}
			else
			{
				rh->Wait(500);
				no_frame_cnt++;
				if (no_frame_cnt > 20)
				{
					PTRACE(2, "No frames from camera");
					break;
				}
			}
		}
		if(frame)
			rh->DeleteFrame(frame);
		PTRACE(3, "Stop Request");
		m_server.DelRequest(rh);
		delete rh;
	} catch (...)
	{
		PTRACE(1, "HTTPMPEG4Resource Send Data Exception");
	}
	m_camera->StopCamera();
}

PBoolean HTTPMPEG4Resource::GetExpirationDate(PTime & when ///< Time that the resource expires
		)
{
	when = ImmediateExpiryTime;
	return PTrue;
}

//=====================================================
HTTPMJPEGResource::HTTPMJPEGResource(WAPAHTTPServer &server,
		WAPAHTTPCamera *camera, const PURL & url, ///< Name of the resource in URL space.
		const PHTTPAuthority & auth, ///< Authorisation for the resource.
		unsigned char ftype) :
		PHTTPResource(url, auth), m_server(server), m_camera(camera)
{
	m_boundary = "myboundary";
	m_ftype = ftype;
	contentType = "multipart/x-mixed-replace; boundary=--" + m_boundary;
}

PBoolean HTTPMJPEGResource::LoadHeaders(PHTTPRequest & request)
{
	request.contentSize = P_MAX_INDEX;
	return PTrue;
}

HTTPMJPEGResource::~HTTPMJPEGResource()
{
}

void HTTPMJPEGResource::SendData(PHTTPRequest & request ///< information for this request
		)
{
	const PStringOptions &query = request.url.GetQueryVars();
	int interval = 0;
	if (query.Has("fr10"))
	{
		int framerate = atoi(query.Get("fr10"));
		if (framerate >= 10)
		{
			interval = 10000 / framerate;
		}
		PTRACE(2, "Request interval "<<interval);
	}
	request.outMIME.SetAt(PHTTP::ContentTypeTag(), contentType);
	request.outMIME.SetAt(PHTTP::ConnectionTag(), "Close");
	request.outMIME.RemoveAt(PHTTP::ContentLengthTag());
	request.outMIME.RemoveAt(PHTTP::DateTag());
	request.outMIME.RemoveAt(PHTTP::ExpiresTag());

	PCharArray data;

	request.server << "HTTP/1.0 200 OK\r\n";
	request.server << setfill('\r') << request.outMIME;

	m_camera->StartCamera();

	try
	{
		request.server.flush();
		int socket_handle = request.server.GetSocket()->GetHandle();
		RequestHolder *rh = new RequestHolder(m_ftype, m_camera, interval);
		m_server.AddRequest(rh);
		PTRACE(3, "Start Request");

		bool wait = false;
		timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 200000;
		PInt64 no_frame_time = 0;
		FrameHolder *frame = NULL;
		bool header_sended = false;
		unsigned int frame_position = 0;

		while (!rh->isExit())
		{
			if (!frame)
				frame = rh->GetFrameAndRemove();
			if (frame)
			{
				no_frame_time = 0;
				int retval;
				if (wait)
				{
					fd_set writefds;
					FD_ZERO(&writefds);
					FD_SET(socket_handle, &writefds);

					tv.tv_sec = 0;
					tv.tv_usec = 200000;
					retval = ::select(socket_handle + 1, NULL, &writefds, NULL, &tv);
				}
				else
				{
					retval = 1;
					usleep(1000);
				}
				if (retval > 0)
				{
					if (!header_sended)
					{
						header_sended = true;
						PString header;
						header = "--" + m_boundary +
								"\r\nContent-Type: image/jpeg\r\nContent-length: "+
								PString(frame->m_size) + "\r\n\r\n";
						retval = ::send(socket_handle,
								(const char*) header,
								header.GetLength(), 0);

					}
					else
					    retval = 1;
//					request.server.flush();

					if (frame_position < frame->m_size)
						retval = ::send(socket_handle,
								(char*) &frame->m_frame[frame_position],
								frame->m_size - frame_position, 0);
					else
						retval = 0;
					if ((retval > 0) || (frame_position >= frame->m_size))
					{
						wait = false;
						frame_position += retval;
						if (frame_position >= frame->m_size)
						{
							header_sended = false;
							rh->DeleteFrame(frame);
							frame = NULL;
							frame_position = 0;
							request.server << "\r\n";
							request.server.flush();
						}
					}
					else
					{
						if (retval == 0)
							wait = true;
						else
						{
							int e = errno;
							if (e != EWOULDBLOCK)
							{
								PTRACE(2, "Socket write error "<<e);
								break;
							}
							else
								wait = true;
						}
					}

				}
			}
			else
			{
				rh->Wait(500);
				PInt64 now = PTimer::Tick().GetMilliSeconds();
				if (no_frame_time)
				{
					if ((now - no_frame_time) > 5000)
					{
						PTRACE(2, "No frames from camera");
						break;
					}
				}
				else
					no_frame_time = now;
			}
		}
		if (frame)
			rh->DeleteFrame(frame);
		PTRACE(3, "Stop Request");
		m_server.DelRequest(rh);
		delete rh;
	} catch (...)
	{
		PTRACE(1, "HTTPMJPEGResource Send Data Exception");
	}
	m_camera->StopCamera();
}

PBoolean HTTPMJPEGResource::GetExpirationDate(PTime & when ///< Time that the resource expires
		)
{
	when = ImmediateExpiryTime;
	return PTrue;
}

//=====================================================
HTTPJPEGResource::HTTPJPEGResource(WAPAHTTPServer &server,
		WAPAHTTPCamera *camera, const PURL & url, ///< Name of the resource in URL space.
		const PHTTPAuthority & auth, ///< Authorisation for the resource.
		unsigned char ftype) :
		PHTTPResource(url, auth), m_server(server), m_camera(camera)
{
	m_ftype = ftype;
	contentType = "image/jpeg";
}

PBoolean HTTPJPEGResource::LoadHeaders(PHTTPRequest & request)
{
	request.contentSize = P_MAX_INDEX;
	return PTrue;
}

HTTPJPEGResource::~HTTPJPEGResource()
{
}

void HTTPJPEGResource::SendData(PHTTPRequest & request ///< information for this request
		)
{
	request.outMIME.SetAt(PHTTP::ContentTypeTag(), contentType);
	request.outMIME.SetAt(PHTTP::ConnectionTag(), "Close");
	request.outMIME.RemoveAt(PHTTP::ContentLengthTag());
	request.outMIME.RemoveAt(PHTTP::DateTag());
	request.outMIME.RemoveAt(PHTTP::ExpiresTag());

	PCharArray data;

	request.server << "HTTP/1.0 200 OK\r\n";
	request.server << setfill('\r') << request.outMIME;

	m_camera->StartCamera();

	try
	{
		request.server.flush();
		int socket_handle = request.server.GetSocket()->GetHandle();
		RequestHolder *rh = new RequestHolder(m_ftype, m_camera);
		m_server.AddRequest(rh);
		PTRACE(3, "Start Request");

		bool wait = false;
		timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 200000;
		PInt64 no_frame_time = PTimer::Tick().GetMilliSeconds();
		FrameHolder *frame = NULL;
		unsigned int frame_position = 0;

		while (!rh->isExit())
		{
			if (!frame)
				frame = rh->GetFrameAndRemove();
			if (frame)
			{
				no_frame_time = 0;
				int retval;
				if (wait)
				{
					fd_set writefds;
					FD_ZERO(&writefds);
					FD_SET(socket_handle, &writefds);

					tv.tv_sec = 0;
					tv.tv_usec = 200000;

					retval = ::select(socket_handle + 1, NULL, &writefds, NULL, &tv);
				}
				else
				{
					retval = 1;
					usleep(1000);
				}
				if (retval > 0)
				{
					if (frame_position < frame->m_size)
						retval = ::send(socket_handle,
								(char*) &frame->m_frame[frame_position],
								frame->m_size - frame_position, 0);
					else
						retval = 0;
					if ((retval > 0) || (frame_position >= frame->m_size))
					{
						wait = false;
						frame_position += retval;
						if (frame_position >= frame->m_size)
						{
							rh->DeleteFrame(frame);
							frame_position = 0;
							frame = NULL;
							break;
						}
					}
					else
					{
						if (retval == 0)
							wait = true;
						else
						{
							int e = errno;
							if (e != EWOULDBLOCK)
							{
								PTRACE(2, "Socket write error "<<e);
								break;
							}
							else
								wait = true;
						}
					}
				}
			}
			else
			{
				rh->Wait(500);
				PInt64 now = PTimer::Tick().GetMilliSeconds();
				if (no_frame_time)
				{
					if ((now - no_frame_time) > 5000)
					{
						PTRACE(2, "No frames from camera");
						break;
					}
				}
				else
					no_frame_time = now;
			}
		}
		if (frame)
			rh->DeleteFrame(frame);
		PTRACE(3, "Stop Request");
		m_server.DelRequest(rh);
		delete rh;
	} catch (...)
	{
		PTRACE(1, "HTTPMJPEGResource Send Data Exception");
	}
	m_camera->StopCamera();
}

PBoolean HTTPJPEGResource::GetExpirationDate(PTime & when ///< Time that the resource expires
		)
{
	when = ImmediateExpiryTime;
	return PTrue;
}
//=====================================================
HTTPStreamingResource::HTTPStreamingResource(WAPAHTTPServer &server,
		WAPAHTTPCamera *camera, const PURL & url, ///< Name of the resource in URL space.
		const PHTTPAuthority & auth, ///< Authorisation for the resource.
		unsigned char ftype,PString content_type) :
		PHTTPResource(url, auth), m_server(server), m_camera(camera)
{
	m_ftype = ftype;
	contentType = content_type;
}

PBoolean HTTPStreamingResource::LoadHeaders(PHTTPRequest & request)
{
	request.contentSize = P_MAX_INDEX;
	return PTrue;
}

HTTPStreamingResource::~HTTPStreamingResource()
{
}

void HTTPStreamingResource::SendData(PHTTPRequest & request ///< information for this request
		)
{
	request.outMIME.SetAt(PHTTP::ContentTypeTag(), contentType);
	request.outMIME.SetAt(PHTTP::ConnectionTag(), "Close");
	request.outMIME.RemoveAt(PHTTP::ContentLengthTag());
	request.outMIME.RemoveAt(PHTTP::DateTag());
	request.outMIME.RemoveAt(PHTTP::ExpiresTag());

	PCharArray data;

	request.server << "HTTP/1.0 200 OK\r\n";
	request.server << setfill('\r') << request.outMIME;

	m_camera->StartCamera();

	try
	{
		request.server.flush();
		int socket_handle = request.server.GetSocket()->GetHandle();
		RequestHolder *rh = new RequestHolder(m_ftype, m_camera);
		m_server.AddRequest(rh);
		PTRACE(3, "Start Request");

		bool wait = false;
		timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 200000;
		int no_frame_cnt = 0;
		FrameHolder *frame = NULL;
		unsigned int frame_position = 0;

		while (!rh->isExit())
		{
			if (!frame)
				frame = rh->GetFrameAndRemove();
			if (frame)
			{
				no_frame_cnt = 0;
				int retval;
				if (wait)
				{
					fd_set writefds;
					FD_ZERO(&writefds);
					FD_SET(socket_handle, &writefds);

					tv.tv_sec = 0;
					tv.tv_usec = 200000;

					retval = ::select(socket_handle + 1, NULL, &writefds, NULL, &tv);
				}
				else
				{
					usleep(1000);
					retval = 1;
				}
				if (retval > 0)
				{
					if (frame_position < frame->m_size)
						retval = ::send(socket_handle,
								(char*) &frame->m_frame[frame_position],
								frame->m_size - frame_position, 0);
					else
						retval = 0;
					if ((retval > 0) || (frame_position >= frame->m_size))
					{
						wait = false;
						frame_position += retval;
						if (frame_position >= frame->m_size)
						{
							rh->DeleteFrame(frame);
							frame_position = 0;
							frame = NULL;
						}
					}
					else
					{
						if (retval == 0)
							wait = true;
						else
						{
							int e = errno;
							if (e != EWOULDBLOCK)
							{
								PTRACE(2, "Socket write error "<<e);
								break;
							}
							else
								wait = true;
						}
					}

				}
			}
			else
			{
				rh->Wait(500);
				no_frame_cnt++;
				if (no_frame_cnt > 20)
				{
					PTRACE(2, "No frames from camera");
					break;
				}
			}
		}
		if(frame)
			rh->DeleteFrame(frame);
		PTRACE(3, "Stop Request");
		m_server.DelRequest(rh);
		delete rh;
	} catch (...)
	{
		PTRACE(1, "HTTPStreamingResource Send Data Exception");
	}
	m_camera->StopCamera();
}

PBoolean HTTPStreamingResource::GetExpirationDate(PTime & when ///< Time that the resource expires
		)
{
	when = ImmediateExpiryTime;
	return PTrue;
}


//=====================================================
void WAPAHTTPCamera::StartCamera()
{
	PWaitAndSignal lock(m_camera_mutex);
	if (!m_camera)
	{
		switch(m_parameters->m_extra.m_type)
		{
			case CT_UVC:
				m_camera = new WAPAUVCCamera(this, m_parameters->m_device, m_parameters->m_device_params, m_parameters->m_extra);
				break;

			case CT_SOUND_ZMQ:
				m_camera = new WAPAZMQSound(this, m_parameters->m_device, m_parameters->m_device_params, m_parameters->m_extra);
				break;


			default:
				m_camera = new WAPAIPCamera(this, PIPSocket::Address(m_parameters->m_camera_address), m_parameters->m_camera_port, m_parameters->m_extra);
				break;
		}
		m_camera->Start();
		PTRACE(2, "Create Camera "<<m_name);
	}
	m_camera_usage++;
	PTRACE(4, "Start Camera "<<m_name);
}

void WAPAHTTPCamera::StopCamera()
{
	PWaitAndSignal lock(m_camera_mutex);
	m_camera_usage--;
	if (m_camera_usage == 0)
	{
		m_camera_stop_time = PTime();
	}
	PTRACE(4, "Stop Camera "<<m_name);
}

void WAPAHTTPCamera::Tick()
{
	if (m_camera)
	{
		if (m_camera_usage <= 0)
		{
			if ((PTime() - m_camera_stop_time).GetMilliSeconds() >= CAMERA_STOP_THRESHOLD_TIME)
			{
				InternalStopCamera();
			}
		}
	}
}

void WAPAHTTPCamera::InternalStopCamera()
{
	PWaitAndSignal lock(m_camera_mutex);
	if (m_camera)
	{
		PTRACE(2, "Internal Stop Camera "<<m_camera);
		m_camera->Stop();
		delete m_camera;
		m_camera = NULL;
		m_camera_usage = 0;
	}
}

void WAPAHTTPCamera::OnVideoFrame(WAPACamera *camera, unsigned char type,
		unsigned int width, unsigned int height, unsigned char *data, int size,
		int interval)
{
	m_server.OnVideoFrame(this, type, width, height, data, size, interval);
}

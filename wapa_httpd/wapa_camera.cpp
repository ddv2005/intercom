#include "wapa_camera.h"

typedef int64_t clock_time_t;

clock_time_t get_tick_count()
{
	return PTimer::Tick().GetMilliSeconds();
}

class avr_counter
{
protected:
	int m_total;
	int m_num;
	int m_wma_div;
	int m_avr;
	int m_len;
	int *m_old_values;
	int m_old_values_pos;
	bool m_empty;
public:
	avr_counter(int len) :
			m_len(len)
	{
		m_old_values = (int *) malloc(sizeof(int) * m_len);
		m_wma_div = 0;
		for (int i = 1; i <= len; i++)
			m_wma_div += i;
		reset();
	}

	~avr_counter()
	{
		free(m_old_values);
	}

	int get_value()
	{
		return get_raw_value() / get_divider();
	}
	int get_raw_value()
	{
		return m_avr;
	}
	static int get_divider()
	{
		return 256;
	}

	void reset()
	{
		m_avr = m_total = m_num = 0;
		m_empty = true;
		m_old_values_pos = 0;
		memset(m_old_values, 0, sizeof(int) * m_len);
	}
	bool is_empty()
	{
		return m_empty;
	}
	void add_value(int32_t value)
	{
		value *= get_divider();
		if (is_empty())
		{
			m_total = value * m_len;
			m_num = value * m_wma_div;
			for (int i = 0; i < m_len; i++)
				m_old_values[i] = value;
			m_avr = value;
		}
		else
		{
			m_total += value - m_old_values[m_old_values_pos];
			m_old_values[m_old_values_pos] = value;
			m_old_values_pos++;
			if (m_old_values_pos >= m_len)
				m_old_values_pos = 0;
			m_num += m_len * value - m_total;
			m_avr = m_num / m_wma_div;
		}
		m_empty = false;
		if (m_avr < 0)
			m_avr = 0;
	}
};

WAPAIPCamera::WAPAIPCamera(WAPACameraCallback *callback,
		PIPSocket::Address address, WORD port, extra_camera_params_t &extra_params) :
		WAPACamera(callback), m_address(address), m_port(port), m_extra_params(
				extra_params)
{
	m_thread = NULL;
}

WAPAIPCamera::~WAPAIPCamera()
{
	Stop();
}

int WAPAIPCamera::Start()
{
	int result = WR_ERROR;
	if (!m_thread)
	{
		m_thread = new PCameraThread(*this, &WAPAIPCamera::CameraThread);
		m_thread->Resume();
		result = WR_OK;
	}
	return result;
}

void WAPAIPCamera::Stop()
{
	if (m_thread)
	{
		m_thread->Stop();
		m_thread->WaitForTermination();
		delete m_thread;
		m_thread = NULL;
	}
}

unsigned char *mem_find(const unsigned char *s, const char *find, unsigned slen,
		unsigned flen)
{
	unsigned char c, sc;

	flen--;
	c = *(const unsigned char*) find++;
	do
	{
		do
		{
			sc = *s++;
			if (slen-- < 1)
				return (NULL);
		} while (sc != c);
		if (flen > slen)
			return (NULL);
	} while (memcmp(s, find, flen) != 0);
	s--;
	return ((unsigned char *) s);
}

int str_caseless_cmp(const void *Ptr1, const void *Ptr2, size_t Count)
{
	INT v = 0;
	BYTE *p1 = (BYTE *) Ptr1;
	BYTE *p2 = (BYTE *) Ptr2;

	while (Count-- > 0 && v == 0)
	{
		v = toupper(*(p1++)) - toupper(*(p2++));
	}

	return v;
}

unsigned char *str_caseless_find(const unsigned char *s, const char *find,
		unsigned slen, unsigned flen)
{
	unsigned char c, sc;

	flen--;
	c = toupper(*find++);
	do
	{
		do
		{
			sc = toupper(*s++);
			if (slen-- < 1)
				return (NULL);
		} while (sc != c);
		if (flen > slen)
			return (NULL);
	} while (str_caseless_cmp(s, find, flen) != 0);
	s--;
	return ((unsigned char *) s);
}

void WAPAIPCamera::CameraThread(PCameraThread*thread)
{
	PTRACE(3, "WAPA Camera Thread Begin");
	PSyncPoint exitSignal = thread->GetExitSignal();

	bool isConnected = false;
	int state = 0;
	unsigned char *buffer = (unsigned char *)malloc(MAX_CAMERA_BUFFER);
	int position = 0;
	int size = 0;
	unsigned char ftype = 0xFF;
	PTCPSocket *client = NULL;
	unsigned int hd_width, hd_height, cif_width, cif_height;
	hd_width = hd_height = cif_width = cif_height = 0;
	PString boundary;
	avr_counter interval_avr_counter(30);
	clock_time_t last_frame_time = 0;
	clock_time_t start_time = 0;

	while (!thread->IsStop())
	{
		bool disconnect = false;
		try
		{
			if (!client)
			{
				last_frame_time = 0;
				interval_avr_counter.reset();
				hd_width = hd_height = cif_width = cif_height = 0;
				state = 0;
				boundary = "";
				switch(m_extra_params.m_type)
				{
					case CT_MPEG4_WAPA:
					size=16;
					break;
					case CT_JPEG_GST:
					size=MAX_JPEG_HEADERS_SIZE;
					break;
				}
				position = 0;
				client = new PTCPSocket(m_port);
				PTRACE(2,"Connecting to camera "<<m_address<<":"<<m_port);
				if (client->Connect(m_address))
				{
					start_time = get_tick_count();
					PTRACE(2,"Connected to camera");
					isConnected = true;
				}
				else
				disconnect = true;
			}
			if (client && isConnected && start_time)
			{
				clock_time_t now = get_tick_count();
				if((now-start_time)>5000)
				{
					isConnected = false;
					disconnect = true;
					PTRACE(2,"Camera timeout");
				}
			}
			if (client && isConnected)
			{
				if (position < size)
				{
					timeval tv;
					tv.tv_sec = 0;
					tv.tv_usec = 100000;
					SOCKET maxfds = client->GetHandle();

					fd_set readfds;
					FD_ZERO(&readfds);
					FD_SET(maxfds, &readfds);

					int retval = ::select(maxfds + 1, &readfds, NULL, NULL,
							&tv);
					if (retval > 0)
					{
						if(m_extra_params.m_type==CT_MPEG4_WAPA)
						{
							bool first = true;
							int cnt = 1;

							while ((cnt > 0)&&!thread->IsStop())
							{
								cnt = ::recv(maxfds, (char *)&buffer[position],
										size - position, 0);
								if (cnt >= 0)
								{
									if (cnt)
									{
										first = false;
										position += cnt;
										if (position >= size)
										{
											position = 0;
											switch (state)
											{
												case 0:
												{
													if ((buffer[0] == 0x7)
															&& (buffer[1] == 0x61))
													{
														hd_width =
														buffer[4]
														+ (((unsigned int) (buffer[5]))
																<< 8);
														hd_height =
														buffer[6]
														+ (((unsigned int) (buffer[7]))
																<< 8);
														cif_width =
														buffer[8]
														+ (((unsigned int) (buffer[9]))
																<< 8);
														cif_height =
														buffer[10]
														+ (((unsigned int) (buffer[11]))
																<< 8);
														state = 1;
													}
													else
													disconnect = true;
													break;
												}
												case 1:
												{
													ftype = buffer[0];
													size =
													buffer[4]
													+ (((unsigned int) (buffer[5]))
															<< 8)
													+ (((unsigned int) (buffer[6]))
															<< 16)
													+ (((unsigned int) (buffer[7]))
															<< 24);
													if (size <= MAX_CAMERA_BUFFER)
													{
														state = 2;
													}
													else
													disconnect = true;
													break;
												}

												case 2:
												{
													unsigned int width, height;
													width = height = 0;
													switch (ftype)
													{
														case 0x80:
														{
															width = hd_width;
															height = hd_height;
															break;
														}
														case 0x0:
														{
															width = hd_width;
															height = hd_height;
															break;
														}
														case 0x82:
														{
															width = cif_width;
															height = cif_height;
															break;
														}
														case 0x02:
														{
															width = cif_width;
															height = cif_height;
															break;
														}
													}
													start_time = get_tick_count();
													if (m_callback)
													try
													{
														m_callback->OnVideoFrame(
																this, ftype,
																width, height,
																buffer, size,0);
													}
													catch (...)
													{
														PTRACE( 1,
																"OnVideoFrame error");
														disconnect = true;
													}

													size = 16;
													state = 1;
													break;
												}
											}
										}
									}
									else
									{
										if (first)
										disconnect = true;
									}
								}
								else
								{
									int e = errno;
									if (e != EWOULDBLOCK)
									disconnect = true;
								}
							}
						}
						else if (m_extra_params.m_type == CT_JPEG_GST)
						{
							bool first = true;
							int cnt = 1;

							while ((cnt > 0) && !thread->IsStop())
							{
								if(size==position)
								position = 0;
								cnt = ::recv(maxfds, (char *) &buffer[position],
										size-position, 0);
								if (cnt >= 0)
								{
									if (cnt)
									{
										first = false;
										position += cnt;

										switch (state)
										{
											case 0: //Looking for boundary marker
		{
			unsigned restart = position-1;
			unsigned char *fpos = mem_find(buffer,"--",position,2);
			if(fpos)
			{
				restart=fpos-buffer;
				state = 2;
			}
			if(restart)
			{
				memmove(buffer,&buffer[restart],position-restart);
				position -= restart;
			}
			break;
		}
		case 1: // Find header
		{
			unsigned restart = position-1;
			if(boundary.IsEmpty())
			state = 0;
			else
			{
				unsigned char *fpos = mem_find(buffer,boundary, position,boundary.GetLength());
				if(fpos)
				{
					restart=fpos-buffer;
					state = 2;
				}
			}
			size = MAX_JPEG_HEADERS_SIZE;
			if(restart)
			{
				memmove(buffer,&buffer[restart],position-restart);
				position -= restart;
			}
			break;
		}
		case 2: //Parse header at 0
		{
			unsigned restart = position-1;
			if((buffer[0]=='-')&&(buffer[1]=='-'))
			{
				unsigned char *fpos;
				if(boundary.IsEmpty())
				{
					fpos = mem_find(buffer,"\r\n",position,2);
					if(fpos)
					{
						*fpos = 0;
						boundary = (char *)buffer;
						boundary.Trim();
						PTRACE(1, "WAPA Camera found boundary "<<boundary);
					}
					else
					{
						state = 0;
					}
				}
				if(!boundary.IsEmpty())
				{
					fpos = str_caseless_find(buffer,"CONTENT-LENGTH:",position,15);
					if(fpos)
					{
						unsigned char *fend = mem_find(fpos,"\r\n",position,2);
						if(fend)
						{
							fpos+=15;
							PString len_str;
							while(fpos<fend)
							{
								if(isdigit(*fpos))
								len_str += *fpos;
								fpos++;
							}
							size = atoi(len_str);
							if(size)
							{
								fpos = mem_find(fpos,"\r\n\r\n",position,4);
								if(fpos)
								{
									restart=fpos-buffer+4;
									state = 3;
								}
								else
								{
									state = 1;
									PTRACE(1, "WAPA Camera can't find end of header");
								}
							}
							else
							{
								size = 1;
								PTRACE(1, "WAPA Camera wrong frame size");
							}
						}
						else
						{
							state = 1;
							PTRACE(1, "WAPA Camera can't find end of line");
						}
					}
					else
					{
						state = 1;
						PTRACE(1, "WAPA Camera can't find content-lenght");
					}
				}
			}
			else
			{
				state = 1;
				PTRACE(1, "WAPA Camera wrong header start");
			}
			if(state!=3)
			size = MAX_JPEG_HEADERS_SIZE;
			if(restart)
			{
				memmove(buffer,&buffer[restart],position-restart);
				position -= restart;
			}
			break;
		}
		case 3: //Get image
		{
			if(position>=size)
			{
				clock_time_t now = get_tick_count();
				int interval = 0;
				if(last_frame_time)
				{
					interval_avr_counter.add_value(now-last_frame_time);
					interval = interval_avr_counter.get_value();
				}
				start_time = last_frame_time = now;
				if (m_callback)
				try
				{
					m_callback->OnVideoFrame(
							this, JPEG_TYPE,
							0, 0,
							buffer, size,interval);
				}
				catch (...)
				{
					PTRACE( 1,
							"OnVideoFrame error");
					disconnect = true;
				}
				position = 0;
				state = 1;
				size = MAX_JPEG_HEADERS_SIZE;
			}
			break;
		}

	}
}
else
{
	if (first)
	disconnect = true;
}
}
else
{
int e = errno;
if (e != EWOULDBLOCK)
disconnect = true;
}
}
}
}
}
else
disconnect = true;
}
}
catch (...)
{
PTRACE(1, "WAPA Camera Thread Exception");
disconnect = true;
}
if(disconnect)
{
PTRACE(1,"Disconnect from camera");
isConnected = false;
if(client)
{
try
{
client->Close();
delete client;
}
catch(...)
{
}
client = NULL;
}
exitSignal.Wait(5000);
}
}
free(buffer);
if(client)
{
try
{
client->Close();
delete client;
}
catch(...)
{
}
client = NULL;
}
PTRACE(3, "WAPA Camera Thread End");
}

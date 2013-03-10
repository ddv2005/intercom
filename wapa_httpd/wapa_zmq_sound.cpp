#include "wapa_zmq_sound.h"

WAPAZMQSound::WAPAZMQSound(WAPACameraCallback *callback,PString device, PString device_params, extra_camera_params_t &extra_params):
WAPACamera(callback), m_device(device), m_device_params(device_params)
{
	m_thread = NULL;
}

int WAPAZMQSound::Start()
{
	int result = WR_ERROR;
	if (!m_thread)
	{
		m_thread = new PZMQSoundThread(*this, &WAPAZMQSound::SoundThread);
		m_thread->Resume();
		result = WR_OK;
	}
	return result;
}

void WAPAZMQSound::Stop()
{
	if (m_thread)
	{
		m_thread->Stop();
		m_thread->WaitForTermination();
		delete m_thread;
		m_thread = NULL;
	}
}

#define HEADER_SIZE (sizeof(pjs_audio_monitor_zeromq_packet_t)+PJSAMZ_TYPE_SIZE)
extern const unsigned char pjmedia_linear2alaw_tab[16384];
#define pjmedia_linear2alaw(pcm_val)	\
	    pjmedia_linear2alaw_tab[(((short)pcm_val) >> 2) & 0x3fff]

extern const unsigned char pjmedia_linear2ulaw_tab[16384];
#define pjmedia_linear2ulaw(pcm_val)	\
	    pjmedia_linear2ulaw_tab[(((short)pcm_val) >> 2) & 0x3fff]


void WAPAZMQSound::SoundThread(PZMQSoundThread *thread)
{
	void *context = zmq_ctx_new();

	void *subscriber = zmq_socket(context, ZMQ_SUB);
	int rc = zmq_connect(subscriber, m_device);
	if (rc == 0)
	{
		rc = zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, NULL, 0);
		if (rc == 0)
		{
			bool f_exit = false;

			unsigned char * buffer = (unsigned char*) calloc(4096,1);
			zmq_pollitem_t items[] =
			{
			{ subscriber, 0, ZMQ_POLLIN, 0 } };
			while (!f_exit && !thread->IsStop())
			{
				zmq_msg_t message;
				zmq_poll(items, 1, 100);
				if (items[0].revents & ZMQ_POLLIN)
				{
					zmq_msg_init(&message);
					zmq_msg_recv(&message, subscriber, 0);

					size_t size = zmq_msg_size(&message);
					char *data = (char *) zmq_msg_data(&message);
					if (size > HEADER_SIZE)
					{
						data += HEADER_SIZE;
						int count = (size - HEADER_SIZE)/2;
						for(int i=0; i<count;i++)
						{
							buffer[i] = pjmedia_linear2ulaw(*(short*)data);
							data+=sizeof(short);
						}
						data = (char *) zmq_msg_data(&message);
						pjs_audio_monitor_zeromq_packet_t *header = (pjs_audio_monitor_zeromq_packet_t *)(data+PJSAMZ_TYPE_SIZE);
						if(m_callback)
							m_callback->OnVideoFrame(this,SND_MULAW_TYPE,header->clock_rate,header->channels,buffer,count,header->clock_rate);
					}
					zmq_msg_close(&message);
				}
			}
			free(buffer);
		}
		else
			PTRACE(0,"ZMQ Connect error "<<rc);
	}
	else
		PTRACE(0,"ZMQ Context error "<<rc);

	zmq_close(subscriber);
	zmq_ctx_destroy(context);
}

WAPAZMQSound::~WAPAZMQSound()
{
}

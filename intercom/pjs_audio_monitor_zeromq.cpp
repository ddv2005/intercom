#include "pjs_audio_monitor_zeromq.h"
#include "pjs_zeromq_common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

pjs_audio_monitor_zeromq::pjs_audio_monitor_zeromq(const char *bind,pjs_global &global,unsigned output_clock_rate, unsigned input_clock_rate, unsigned input_samples_per_frame,pj_uint8_t gain_max)
:pjs_audio_monitor(global,output_clock_rate,input_clock_rate,input_samples_per_frame,gain_max)
{
	m_context = NULL;
	m_socket = NULL;

	if(bind && bind[0])
	{
		m_context = zmq_ctx_new ();
		m_socket = zmq_socket (m_context, ZMQ_PUB);

		char *vbind = strdup(bind);
		if(vbind)
		{
			char *saveptr1;
			char *token = strtok_r(vbind,";",&saveptr1);
			while(token)
			{
				int rc = zmq_bind(m_socket,token);
				if(rc!=0)
					PJ_LOG_(ERROR_LEVEL, (__FILE__,"zmq_bind error %d",rc));

				token = strtok_r(NULL,";",&saveptr1);
			}
			free(vbind);
		}
	}
}

pjs_audio_monitor_zeromq::~pjs_audio_monitor_zeromq()
{
	stop_monitor();
	if(m_socket)
		zmq_close (m_socket);
	if(m_context)
		zmq_ctx_destroy (m_context);
}

void pjs_audio_monitor_zeromq::on_process_frame(const pjmedia_frame *frame)
{
	if(m_socket && frame && (frame->type==PJMEDIA_FRAME_TYPE_AUDIO) && frame->buf && frame->size && (frame->size<1400))
	{
		size_t size = sizeof(pjs_audio_monitor_zeromq_packet_t)+PJSAMZ_TYPE_SIZE+frame->size;
    zmq_msg_t message;
    zmq_msg_init_size (&message, size);
    char * data = (char *)zmq_msg_data (&message);
    *data = PJSAMZ_TYPE_WAVE;
    data+=PJSAMZ_TYPE_SIZE;

    ((pjs_audio_monitor_zeromq_packet_t *)data)->channels = 1;
    ((pjs_audio_monitor_zeromq_packet_t *)data)->clock_rate = m_clock_rate;
    data+=sizeof(pjs_audio_monitor_zeromq_packet_t);

    memcpy(data,frame->buf,frame->size);

    zmq_msg_send (&message, m_socket, 0);
    zmq_msg_close (&message);
	}
}

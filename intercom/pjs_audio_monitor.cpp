#include "pjs_audio_monitor.h"

pjs_audio_monitor::pjs_audio_monitor(pjs_global &global,
		unsigned output_clock_rate, unsigned input_clock_rate,
		unsigned input_samples_per_frame,pj_uint8_t gain_max) :
		pjs_port("audio_monitor", PJMEDIA_PORT_SIGNATURE('P','J','A','M'),
				output_clock_rate, 1,
				input_samples_per_frame * output_clock_rate / input_clock_rate, 16, 0), pjs_frame_processor(
				global, m_port.info.samples_per_frame * 4,
				m_port.info.samples_per_frame)
{
	m_processor = NULL;
	m_samples_per_frame = m_port.info.samples_per_frame;
	m_resample = NULL;
	m_clock_rate = output_clock_rate;
	m_ready = false;
	m_input_id = 0;
	if (pjs_frame_processor::m_ready)
	{
		bool b = true;
		if (input_clock_rate != output_clock_rate)
		{
			if (pjmedia_resample_port_create(m_pool, get_mediaport(),
					output_clock_rate,
					PJMEDIA_RESAMPLE_DONT_DESTROY_DN | PJMEDIA_RESAMPLE_USE_SMALL_FILTER,
					&m_resample) != PJ_SUCCESS)
				b = false;
		}
		if (b
				&& pjsua_conf_add_port(m_pool,
						m_resample ? m_resample : get_mediaport(), &m_input_id)
						== PJ_SUCCESS)
		{
			m_processor = new pjs_agc("audio monitor agc",gain_max,1,32760,32760);
			m_ready = true;
		}
		else
			m_input_id = 0;
	}
}

pjs_audio_monitor::~pjs_audio_monitor()
{
	stop_monitor();
	if(m_processor)
		delete m_processor;
	if (m_input_id)
		pjsua_conf_remove_port(m_input_id);
}

inline pj_status_t pjs_audio_monitor::put_frame(const pjmedia_frame *frame)
{
	if(frame&&frame->buf&&frame->size&&(frame->type==PJMEDIA_FRAME_TYPE_AUDIO))
		if(m_processor)
			m_processor->process((pj_int16_t*)frame->buf,frame->size/2,0,NULL);

	return process_frame(frame);
}


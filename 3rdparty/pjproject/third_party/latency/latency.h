#ifndef LATENCY_H_
#define LATENCY_H_

#include "common.h"
#include "tones.h"
#include "gaincontrol.h"
#include <pjmedia_audiodev.h>

#define IDLE_FREQ1	1133
#define IDLE_FREQ2	1765
#define ACTIVE_FREQ1	1401
#define ACTIVE_FREQ2	2307

SWIG_BEGIN_DECL
#define MAX_FILENAME_PATH	255

typedef struct
{
	pj_uint8_t	level;
	pj_char_t	file_name[MAX_FILENAME_PATH+1];
	pj_uint8_t 	max_files;
	pj_uint32_t	max_file_size; // in Megabytes;
}log_config_t;

typedef struct
{
	log_config_t logs;
	pj_uint32_t	clock_rate;
	pj_uint16_t	min_frame_length;
}latency_config_t;

latency_config_t create_default_latency_config();

class latency_checker
{
protected:
	pj_pool_t	*m_pool;
	pj_caching_pool *m_caching_pool;
	pj_pool_factory *m_pool_factory;
	latency_config_t m_config;
	pjmedia_aud_stream *m_aud_stream;
	gen_state	m_idle_tone;
	gen_state	m_active_tone;
	tone_detector *m_idle_freq1_det;
	tone_detector *m_idle_freq2_det;
	tone_detector *m_active_freq1_det;
	tone_detector *m_active_freq2_det;
	pj_uint16_t m_dstate; // 0 - wait for idle, 1 - idle detected, 2 - wait for active
	pj_uint16_t m_dcnt;
	pj_timestamp m_start_tone_time;
	pj_timestamp m_last_get_frame_time;
	pj_uint32_t  m_latency;
	PPJ_SemaphoreLock *m_lock;
	snd_agc *m_gain;
	pj_uint16_t m_quality;
	char 		m_status_string[512];

	void internal_clean();
	static pj_status_t rec_cb_s(void *user_data, pjmedia_frame *frame)
	{
		if(user_data)
		{
			return ((latency_checker *)user_data)->put_frame(frame);
		}
		return PJ_SUCCESS;
	}

	static pj_status_t play_cb_s(void *user_data, pjmedia_frame *frame)
	{
		if(user_data)
		{
			return ((latency_checker *)user_data)->get_frame(frame);
		}
		return PJ_SUCCESS;
	}
	pj_status_t get_frame(pjmedia_frame *frame);
	pj_status_t put_frame(const pjmedia_frame *frame);
public:
	latency_checker()
	{
		m_aud_stream = NULL;
		m_pool = NULL;
		m_caching_pool = NULL;
		m_pool_factory = NULL;
		m_idle_freq1_det = m_idle_freq2_det = m_active_freq1_det = m_active_freq2_det = NULL;
		m_latency = 0;
		m_gain = NULL;
		m_quality = 0;
	}
	~latency_checker()
	{
		stop();
	}
	pj_status_t start(latency_config_t &config);
	void stop();
	bool is_started() { return m_pool; }
	pj_uint32_t  get_latency() { pj_uint32_t result = m_latency; m_latency = 0; return result; }
	char *get_status() { return m_status_string; }
	pj_uint16_t get_quality() { return m_quality; }

};

SWIG_END_DECL

#endif /* LATENCY_H_ */

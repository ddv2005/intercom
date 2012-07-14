#include "latency.h"

#include "pj_support.h"
#include "pj_logger.h"
#include <unistd.h>
#if PJ_ANDROID_DEVICE==1
#include "android_dev.h"
#endif

#if PJ_ANDROID_DEVICE==2
#include "opensl_dev.h"
#endif

#if PJ_ANDROID_DEVICE==3
extern "C" pjmedia_aud_dev_factory* pjmedia_alsa_factory(pj_pool_factory *pf);
#endif

#define THIS_FILE	"latency"

latency_config_t create_default_latency_config()
{
	latency_config_t result;
	memset(&result,0,sizeof(result));
	result.clock_rate = 48000;
	result.min_frame_length = 10;

    result.logs.level = DEBUG_LEVEL;
    result.logs.max_file_size = 10;
    result.logs.max_files = 2;

	return result;
}

pj_status_t latency_checker::start(latency_config_t &config)
{
	pj_status_t result = PJ_EINVAL;
	if(m_pool == NULL)
	{
		m_dstate = 0;
		m_start_tone_time.u64 = 0;
		m_latency = 0;
		m_quality = 0;
		m_config = config;
		m_status_string[0] = 0;
		m_gain = new snd_agc("",30,1,32000,32000);
		m_idle_freq1_det = new tone_detector(m_config.clock_rate,IDLE_FREQ1);
		m_idle_freq2_det = new tone_detector(m_config.clock_rate,IDLE_FREQ2);
		m_active_freq1_det = new tone_detector(m_config.clock_rate,ACTIVE_FREQ1);
		m_active_freq2_det = new tone_detector(m_config.clock_rate,ACTIVE_FREQ2);

		init_generate_dual_tone(&m_idle_tone,m_config.clock_rate,IDLE_FREQ1,IDLE_FREQ2,32767);
		init_generate_dual_tone(&m_active_tone,m_config.clock_rate,ACTIVE_FREQ1,ACTIVE_FREQ2,32767);
		pj_log_set_level(0);
		pj_log_set_decor(PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_SENDER | PJ_LOG_HAS_TIME | PJ_LOG_HAS_MICRO_SEC);
		if(pj_init()==PJ_SUCCESS)
		{
			m_caching_pool = (pj_caching_pool *)malloc(sizeof(pj_caching_pool));
			pj_caching_pool_init( m_caching_pool, NULL, 0 );
			m_pool_factory=&m_caching_pool->factory;
			m_pool = pj_pool_create(m_pool_factory, "LATENCY NATIVE", 4000, 4000, NULL);
			pj_log_set_level(m_config.logs.level);
			pj_logging_init(m_pool);
			pj_logging_setLogToConsole(1);

			pj_logging_setFilename(m_config.logs.file_name);
			pj_logging_setMaxLogFiles(m_config.logs.max_files);
			pj_logging_setMaxLogFileSize(m_config.logs.max_file_size*1024*1024);
			pj_logging_start();

			pj_get_timestamp(&m_last_get_frame_time);

			m_lock = new PPJ_SemaphoreLock(m_pool,NULL,1,1);

			pjmedia_aud_subsys_init(m_pool_factory);
#if PJMEDIA_AUDIO_DEV_HAS_ANDROID
#if PJ_ANDROID_DEVICE==1
			pjmedia_aud_register_factory(&pjmedia_android_factory);
#endif
#if PJ_ANDROID_DEVICE==2
			pjmedia_aud_register_factory(&pjmedia_opensl_factory);
#endif
#if PJ_ANDROID_DEVICE==3
			pjmedia_aud_register_factory(&pjmedia_alsa_factory);
#endif
#endif

			pjmedia_aud_param params;
			params.dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
			params.rec_id = 1;
			params.play_id = 6;
			params.clock_rate = m_config.clock_rate;
			params.channel_count = 1;
			params.samples_per_frame = m_config.min_frame_length*m_config.clock_rate/1000;
			params.bits_per_sample = 16;
			params.flags = PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY | PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY;
			params.input_latency_ms = m_config.min_frame_length;
			params.output_latency_ms = m_config.min_frame_length;
			result = pjmedia_aud_stream_create(&params,&rec_cb_s,&play_cb_s,this,&m_aud_stream);

			if(result==PJ_SUCCESS)
			{
				result = pjmedia_aud_stream_start(m_aud_stream);
				if(result==PJ_SUCCESS)
				{


				}

			}
		}
	}

	if(result!=PJ_SUCCESS)
		internal_clean();
	return result;
}

void latency_checker::internal_clean()
{
	if(m_aud_stream)
	{
		pjmedia_aud_stream_stop(m_aud_stream);
		pjmedia_aud_stream_destroy(m_aud_stream);
		m_aud_stream = NULL;
	}
	if(m_pool)
		pjmedia_aud_subsys_shutdown();
	pj_logging_shutdown();
	pj_logging_destroy();
	if(m_lock)
	{
		delete m_lock;
		m_lock = NULL;
	}
	if(m_pool)
	{
		pj_pool_release(m_pool);
		m_pool = NULL;
	}
	if(m_caching_pool)
		pj_caching_pool_destroy(m_caching_pool);
	m_pool_factory = NULL;
	m_caching_pool = NULL;
	pj_shutdown();
	if(m_idle_freq1_det)
	{
		delete m_idle_freq1_det;
		m_idle_freq1_det = NULL;
	}
	if(m_idle_freq2_det)
	{
		delete m_idle_freq2_det;
		m_idle_freq2_det = NULL;
	}
	if(m_active_freq1_det)
	{
		delete m_active_freq1_det;
		m_active_freq1_det = NULL;
	}
	if(m_active_freq2_det)
	{
		delete m_active_freq2_det;
		m_active_freq2_det = NULL;
	}
	if(m_gain)
	{
		delete m_gain;
		m_gain = NULL;
	}
}

pj_status_t latency_checker::get_frame(pjmedia_frame *frame)
{
	PPJ_WaitAndLock lock(*m_lock);
	if(frame&&(frame->buf))
	{
		bool gen_active = false;
		pj_timestamp now;
		pj_get_timestamp(&now);
		switch(m_dstate)
		{
			case 1:
			{
				if((pj_elapsed_msec(&m_start_tone_time,&now)>100)&&(pj_elapsed_msec(&m_last_get_frame_time,&now)>m_config.min_frame_length/2))
				{
					PJ_LOG_(DEBUG_LEVEL,("LATENCY","Starting ACTIVE tone."));
					m_start_tone_time = now;
					m_dstate = 2;
					m_dcnt = 0;
					gen_active = true;
				}
				break;
			}

			case 2:
			{
				gen_active = m_dcnt++<3;
				if(pj_elapsed_msec(&m_start_tone_time,&now)>2000)
				{
					m_dstate = 0;
					PJ_LOG_(DEBUG_LEVEL,("LATENCY","ACTIVE not found. Reseting."));

				}
				break;
			}
		};
		if(gen_active)
		{
			generate_dual_tone(&m_active_tone,1,frame->size/2,(short*)(frame->buf));
		}
		else
		{
			generate_dual_tone(&m_idle_tone,1,frame->size/2,(short*)(frame->buf));
		}

	}
	pj_get_timestamp(&m_last_get_frame_time);
	return PJ_SUCCESS;
}

#define MIN_LEVEL1	50
#define MIN_LEVEL2	500

bool ratio(pj_int32_t target,pj_int32_t i1, pj_int32_t i2)
{
	if(i2==0)
		return true;
	else
		return (i1/i2)>=target;

}
pj_status_t latency_checker::put_frame(const pjmedia_frame *frame)
{
	m_gain->process((short*)frame->buf,frame->size/2,0,NULL);

	PPJ_WaitAndLock lock(*m_lock);
	pj_uint16_t detected = 0;
	pj_int32_t if1,if2,af1,af2;
	if1 = m_idle_freq1_det->detect((short*)frame->buf,frame->size/2);
	if2 = m_idle_freq2_det->detect((short*)frame->buf,frame->size/2);
	af1 = m_active_freq1_det->detect((short*)frame->buf,frame->size/2);
	af2 = m_active_freq2_det->detect((short*)frame->buf,frame->size/2);

	if((if1>MIN_LEVEL1)&&(if2>MIN_LEVEL2)&&ratio(8,if1,af1)&&ratio(5,if1,af2)&&ratio(3,if2,af1)&&ratio(10,if2,af2))
		detected = 1;
	else
		if((af1>MIN_LEVEL1)&&(af2>MIN_LEVEL2)&&ratio(10,af1,if1)&&ratio(10,af1,if2)&&ratio(3,af2,if1)&&ratio(10,af2,if2))
			detected = 2;
	m_quality = m_quality*0.95;
	if(detected)
		m_quality += 5;
	pj_timestamp now;
	pj_get_timestamp(&now);
	switch(m_dstate)
	{
		case 0:
		{
			if(detected==1)
			{
				PJ_LOG_(DEBUG_LEVEL,("LATENCY","Detected IDLE on wait."));
				m_start_tone_time = now;
				m_dstate = 1;
			}
			break;
		}
		case 2:
		{
			if(detected==2)
			{
				m_latency = pj_elapsed_msec(&m_start_tone_time,&now);
				PJ_LOG_(DEBUG_LEVEL,("LATENCY","Detected ACTIVE.Latency = %d", m_latency));				
				m_dstate = 0;
			}
			break;
		}
	};
	sprintf(m_status_string,"TONE: %d (%d, %d, %d, %d)",detected,if1,if2,af1,af2);
	//PJ_LOG_(DEBUG_LEVEL,("LATENCY","rec %d (%d, %d, %d, %d)",detected,if1,if2,af1,af2));
	return PJ_SUCCESS;
}


void latency_checker::stop()
{
	if(m_pool)
		internal_clean();
}

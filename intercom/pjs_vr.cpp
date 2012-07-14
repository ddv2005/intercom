#include "pjs_vr.h"
#include "pjs_gaincontrol.h"

#define THIS_FILE	"pjs_vr.cpp"
pjs_vr::pjs_vr(pjs_global &global, const char *sounds_path,unsigned sampling_rate,
		   unsigned samples_per_frame,pjs_exit_t &exit,pjs_shared_event &exit_event,pjs_vr_callback *callback,void * user_data, pj_int32_t user_param):pjs_port("vr",PJMEDIA_PORT_SIGNATURE('P','J','V','R'),sampling_rate,1,samples_per_frame,16,0),
		   m_global(global),m_exit(exit),m_exit_event(exit_event),m_user_data(user_data),m_user_param(user_param),m_callback(callback)
{
	m_sounds_path = sounds_path;
	if(m_sounds_path.size()>0)
	{
		if(m_sounds_path[m_sounds_path.size()-1]!='/')
			m_sounds_path.push_back('/');
	}
	else
		m_sounds_path = "./";
	m_ptime = (pj_uint32_t)samples_per_frame*(pj_uint32_t)1000/(pj_uint32_t)sampling_rate;
	m_state = VRS_IDLE;
	m_getport = NULL;
	m_getpool = NULL;
	m_getbuffer = NULL;
	m_getresample = NULL;
	m_async = false;
	m_effect = VRE_NONE;
	m_digits_timeout = m_interdigits_timeout = 0;
	m_wait_input_start_time.u64 = m_last_digits_time.u64 = 0;
	m_get_first_time = true;
}

pjs_vr::~pjs_vr()
{
	m_lock.lock();
	internal_stop();
	m_lock.unlock();
}

bool pjs_vr::check_terminate_cond()
{
	if(m_wait_input_max)
	{
		if(m_input_buffer.size()>=m_wait_input_max)
			return true;
	}

	if(!m_input_buffer.empty() && !m_terminate_cond.empty())
	{
		for(unsigned i=0; i<m_terminate_cond.length();i++)
			if(m_input_buffer.find(m_terminate_cond[i])!=string::npos)
				return true;
	}

	if(m_digits_timeout||m_interdigits_timeout)
	{
		pj_timestamp now;
		pj_get_timestamp(&now);
		if(m_digits_timeout)
		{
			if(pj_elapsed_msec(&m_wait_input_start_time,&now)>=m_digits_timeout)
				return true;
		}
		if(m_interdigits_timeout)
		{
			if(pj_elapsed_msec(&m_last_digits_time,&now)>=m_interdigits_timeout)
				return true;
		}
	}
	return false;
}

void pjs_vr::on_input(const char *input)
{
	m_lock.lock();
	pj_get_timestamp(&m_last_digits_time);
	m_input_buffer.append(input);
	if(m_state!=VRS_IDLE)
	{
		if(check_terminate_cond())
			internal_stop();
	}
	m_lock.unlock();
}

void pjs_vr::on_input(const char digit)
{
	m_lock.lock();
	pj_get_timestamp(&m_last_digits_time);
	m_input_buffer.push_back(digit);
	if(m_state!=VRS_IDLE)
	{
		if(check_terminate_cond())
			internal_stop();
	}
	m_lock.unlock();
}

std::string pjs_vr::internal_get_input(pj_uint8_t max_digits,const char *terminate_cond,bool lock)
{
	if(lock)
		m_lock.lock();
	std::string result;
	while((m_input_buffer.size()>0)&&(max_digits>0))
	{
		result.push_back(m_input_buffer[0]);
		m_input_buffer.erase(0,1);
		max_digits--;
		if(m_terminate_cond.find(m_input_buffer[0])!=string::npos)
		{
			break;
		}
	}
	if(lock)
		m_lock.unlock();
	return result;
}

std::string pjs_vr::wait_input(pj_uint8_t max_digits,const char *terminate_cond,pj_uint32_t timeout,pj_uint32_t inter_digits_timeout,bool async, pjs_vr_async_param_t async_param1, pjs_vr_async_param_t async_param2)
{
	std::string result;
	m_api_lock.lock();
	m_lock.lock();
	internal_stop();
	if(!m_exit)
	{
		m_async = async;
		m_digits_timeout = timeout;
		m_interdigits_timeout = inter_digits_timeout;
		m_terminate_cond = terminate_cond;
		m_wait_input_max = max_digits;

		pj_get_timestamp(&m_wait_input_start_time);
		m_last_digits_time = m_wait_input_start_time;
		if(check_terminate_cond())
		{
			async_completed(VRS_WAIT_INPUT);
			if(!async)
			{
				result = internal_get_input(max_digits,terminate_cond,false);
			}
			async = true;
		}
		else
		{
			m_state = VRS_WAIT_INPUT;
		}
	}
	m_lock.unlock();
	if(!async)
	{
		if(!m_exit)
		{
			internal_wait_completion(-1);
			result = internal_get_input(max_digits,terminate_cond,false);
		}
	}
	m_api_lock.unlock();
	return result;
}

void pjs_vr::clear_input()
{
	m_lock.lock();
	m_input_buffer.clear();
	m_lock.unlock();
}

std::string pjs_vr::get_input()
{
	m_lock.lock();
	std::string result(m_input_buffer);
	m_input_buffer.clear();
	m_lock.unlock();
	return result;
}

int	pjs_vr::play_file(const char * filename,const char * terminate_cond, bool loop, bool async, pjs_vr_async_param_t async_param1, pjs_vr_async_param_t async_param2)
{
	int result = -1;
	m_api_lock.lock();
	m_lock.lock();
	internal_stop();
	string filepath = m_sounds_path;
	filepath.append(filename);
	PJ_LOG_(INFO_LEVEL,(THIS_FILE,"play_file %s (%s),%s,%s",filepath.c_str(),terminate_cond,loop?"true":"false",async?"true":"false"));
	m_terminate_cond = terminate_cond;
	m_async = async;
	if(async)
	{
		m_async_param1 = async_param1;
		m_async_param2 = async_param2;
	}
	if(!check_terminate_cond()&&!m_exit)
	{
		m_getpool = pj_pool_create(m_global.get_pool_factory(),"vr",128,128,NULL);
		if(m_getpool)
		{
			pj_status_t status = pjmedia_wav_player_port_create(m_getpool,filepath.c_str(),m_ptime,loop?0:PJMEDIA_FILE_NO_LOOP,0,&m_getport);
			if(status==PJ_SUCCESS)
			{
				m_getsize = m_getport->info.samples_per_frame*m_getport->info.channel_count*m_getport->info.bits_per_sample/8;
				m_getbuffer = (pj_int16_t*)pj_pool_alloc(m_getpool,m_getsize);
				if(m_getbuffer)
				{
					result = 0;
					if(m_getport->info.clock_rate!=m_port.info.clock_rate)
					{
						status = pjmedia_resample_create(m_getpool,true,true,m_getport->info.channel_count,m_getport->info.clock_rate,m_port.info.clock_rate,m_getport->info.samples_per_frame,&m_getresample);
						if(status!=PJ_SUCCESS)
							result = -1;
					}
				}
			}
			else
				m_getport = NULL;
		}
		if(result<0)
			internal_stop();
		else
			m_state = VRS_PLAY_FILE;
	}
	else
	{
		async_completed(VRS_PLAY_FILE);
		async = true;
	}
	m_lock.unlock();
	if(!async&&!m_exit)
	{
		internal_wait_completion(-1);
	}
	m_api_lock.unlock();
	PJ_LOG_(INFO_LEVEL,(THIS_FILE,"play_file exit"));
	return result;
}

void pjs_vr::async_completed(pj_uint8_t state)
{
	if(m_async&&!m_exit&&m_callback)
	{
		m_async = false;
		m_callback->on_async_completed(this,state,m_async_param1,m_async_param2);
	}
	else
		m_async = false;
}

bool pjs_vr::wait_async_completion(pj_int32_t timeout)
{
	return internal_wait_completion(timeout);
}

void pjs_vr::fade_out(pj_uint32_t time_)
{
	m_lock.lock();
	if(m_state==VRS_PLAY_FILE)
	{
		m_effect_param = (int)time_;
		m_effect = VRE_FADE_OUT;
		pj_get_timestamp(&m_effect_start_time);
	}
	m_lock.unlock();

}

bool pjs_vr::internal_wait_completion(pj_int32_t timeout)
{
	bool b = timeout>=0;
	pj_timestamp st,now;
	PJ_LOG_(INFO_LEVEL,(THIS_FILE,"internal_wait_completion %d",timeout));
	pj_uint8_t state = m_state;
	if(b)
		pj_get_timestamp(&st);
	while((state!=VRS_IDLE)&&!m_exit)
	{
		if(b)
		{
			pj_get_timestamp(&now);
			pj_int32_t w = timeout - pj_elapsed_msec(&st,&now);
			if(w>20)
				w=20;
			if(w>0)
				m_completed_event.wait(w);
		}
		else
			m_completed_event.wait(20);
		m_lock.lock();
		state = m_state;
		m_lock.unlock();
		if(b)
		{
			pj_get_timestamp(&now);
			if((int)pj_elapsed_msec(&st,&now)>=timeout)
			{
				PJ_LOG_(INFO_LEVEL,(THIS_FILE,"internal_wait_completion timeout exired"));
				break;
			}
		}
	}
	PJ_LOG_(INFO_LEVEL,(THIS_FILE,"internal_wait_completion exit %s",(state==VRS_IDLE)?"true":"false"));
	return state==VRS_IDLE;
}

void pjs_vr::internal_stop()
{
	async_completed(m_state);
	m_get_first_time = true;
	m_wait_input_max = 0;
	m_interdigits_timeout = 0;
	m_digits_timeout = 0;
	m_effect = VRE_NONE;
	m_completed_event.signal();
	m_terminate_cond.clear();
	if(m_getresample)
	{
		pjmedia_resample_destroy(m_getresample);
		m_getresample = NULL;
	}
	if(m_getport)
	{
		pjmedia_port_destroy(m_getport);
		m_getport = NULL;
	}
	m_getbuffer = NULL;
	if(m_getpool)
	{
		pj_pool_release(m_getpool);
		m_getpool = NULL;
	}
	m_state = VRS_IDLE;
}

void pjs_vr::stop()
{
	m_lock.lock();
	if(m_state != VRS_IDLE)
		PJ_LOG_(INFO_LEVEL,(THIS_FILE,"Stop VR"));
	internal_stop();
	m_lock.unlock();
}


pj_status_t pjs_vr::get_frame(pjmedia_frame *frame)
{
	bool empty = true;
	if((m_state!=VRS_IDLE)&&(frame->size==m_port.info.bytes_per_frame))
	{
		m_lock.lock();
		if(m_getport&&m_getbuffer)
		{
			pjmedia_frame getframe;
			getframe = *frame;
			getframe.buf = m_getbuffer;
			getframe.size = m_getsize;
			if(pjmedia_port_get_frame(m_getport,&getframe)==PJ_SUCCESS)
			{
				if((getframe.type==PJMEDIA_FRAME_TYPE_AUDIO)&&(getframe.size))
				{
					empty = false;
					frame->type = getframe.type;
					if(m_getport->info.channel_count!=m_port.info.channel_count)
						pjmedia_convert_channel_nto1(m_getbuffer,m_getbuffer,m_getport->info.channel_count,m_getport->info.samples_per_frame,true,0);
					if(m_getresample)
					{
						pjmedia_resample_run(m_getresample,m_getbuffer,(pj_int16_t*)frame->buf);
					}
					else
					{
						memcpy(frame->buf,m_getbuffer,frame->size);
					}
					if(m_get_first_time)
					{
						m_get_first_time = false;
						pjs_agc::gain(pjs_agc::DIV(1,1),0,(pj_int16_t*)frame->buf,frame->size >> 1 );
					}
					switch(m_effect)
					{
					case VRE_FADE_OUT:
					{
						if(m_effect_param>0)
						{
							pj_timestamp now;
							pj_get_timestamp(&now);
							if((int)pj_elapsed_msec(&m_effect_start_time,&now)>=m_effect_param)
								empty = true;
							else
							{
								pj_int32_t diff = m_effect_param-pj_elapsed_msec(&m_effect_start_time,&now);
								pj_int32_t gain = pjs_agc::DIV(diff,m_effect_param);
								pjs_agc::gain(gain,gain,(pj_int16_t*)frame->buf,frame->size >> 1 );
							}
						}
						break;
					}
					}
				}
			}
			if(empty)
			{
				internal_stop();
			}
		}
		else
		{
			if(check_terminate_cond())
			{
				internal_stop();
			}
		}
		m_lock.unlock();
	}
	if(empty)
	{
		frame->type = PJMEDIA_FRAME_TYPE_NONE;
		if(frame->buf)
			memset(frame->buf,0,frame->size);
	}
	return PJ_SUCCESS;
}

pj_status_t pjs_vr::put_frame(const pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(frame);
    return PJ_SUCCESS;
}





#ifndef PJS_AUDIO_COMBINE_H_
#define PJS_AUDIO_COMBINE_H_

#include "project_common.h"
#include <pjmedia.h>
#include "project_global.h"
#include "pjs_port.h"
#include "pjs_utils.h"
#include "pjs_media_sync.h"


class pjs_audio_combine:public pjs_media_sync_callback
{
protected:
	pjs_global		&m_global;
	pj_pool_t	 *m_pool;
	pjs_media_sync &m_sync;
	pjmedia_port	* m_combiner;
	pjmedia_port	* m_input1;
	pjmedia_port	* m_input2;
	pjmedia_port	* m_resample1;
	pjmedia_port	* m_resample2;
	pj_int16_t	 *m_getbuffer;
	pj_int16_t	 *m_putbuffer;
	pj_int16_t	  m_buffer_size;
	unsigned  	  m_clock_rate;


	unsigned 	  m_samples_per_frame;
	bool				m_ready;
	bool				m_started;
	bool				m_oneway;
	// start/stop are NOT thread safe
	void start();
	void stop();
	virtual void media_sync(pjs_media_sync_param_t param);
	virtual pj_status_t get_frame(pjmedia_frame *frame){ return PJ_SUCCESS; }
	virtual pj_status_t put_frame(const pjmedia_frame *frame)=0;
public:
	pjs_audio_combine(pjs_global &global,pjs_media_sync &sync,unsigned output_clock_rate, unsigned input_clock_rate, unsigned input_samples_per_frame, bool oneway);
	virtual ~pjs_audio_combine();

	bool is_ready() { return m_ready; }
	pjmedia_port * get_input1() { return m_resample1?m_resample1:m_input1; }
	pjmedia_port * get_input2() { return m_resample2?m_resample2:m_input2; }
	pjmedia_port * get_output() { return m_combiner; }
};


#endif /* PJS_AUDIO_COMBINE_H_ */

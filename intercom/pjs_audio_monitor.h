/*
 * pjs_audio_monitor.h
 *
 *  Created on: Feb 21, 2013
 *      Author: ddv
 */

#ifndef PJS_AUDIO_MONITOR_H_
#define PJS_AUDIO_MONITOR_H_

#include "pjs_audio_combine.h"
#include "pjs_frame_processor.h"
#include "pjs_gaincontrol.h"

class pjs_audio_monitor: public pjs_port, public pjs_frame_processor
{
protected:
	unsigned m_samples_per_frame;
	pjmedia_port * m_resample;
	unsigned m_clock_rate;
	pjsua_conf_port_id m_input_id;
	bool m_ready;
	pjs_agc *m_processor;

	virtual pj_status_t put_frame(const pjmedia_frame *frame);
public:
	pjs_audio_monitor(pjs_global &global, unsigned output_clock_rate,
			unsigned input_clock_rate, unsigned input_samples_per_frame,pj_uint8_t gain_max);
	virtual ~pjs_audio_monitor();

	bool start_monitor()
	{
		if (m_ready)
		{
			pjs_frame_processor::start();
		}
		return m_ready && pjs_frame_processor::m_started;
	}

	void stop_monitor()
	{
		pjs_frame_processor::stop();
	}

	pjsua_conf_port_id get_input1_id()
	{
		return m_input_id;
	}
	pjsua_conf_port_id get_input2_id()
	{
		return m_input_id;
	}
};

#endif /* PJS_AUDIO_MONITOR_H_ */

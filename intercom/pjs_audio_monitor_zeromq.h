/*
 * pjs_audio_monitor_zeromq.h
 *
 *  Created on: Feb 22, 2013
 *      Author: ddv
 */

#ifndef PJS_AUDIO_MONITOR_ZEROMQ_H_
#define PJS_AUDIO_MONITOR_ZEROMQ_H_

#include "pjs_audio_monitor.h"
#include "project_config.h"
#include "zeromq/include/zmq.h"

class pjs_audio_monitor_zeromq:public pjs_audio_monitor
{
protected:
	void * m_context;
	void * m_socket;
	virtual void on_process_frame(const pjmedia_frame *frame);
public:
	pjs_audio_monitor_zeromq(const char *bind,pjs_global &global,unsigned output_clock_rate, unsigned input_clock_rate, unsigned input_samples_per_frame,pj_uint8_t gain_max);
	virtual ~pjs_audio_monitor_zeromq();
};


#endif /* PJS_AUDIO_MONITOR_ZEROMQ_H_ */

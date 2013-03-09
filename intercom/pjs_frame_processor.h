/*
 * pjs_frame_processor.h
 *
 *  Created on: Feb 21, 2013
 *      Author: ddv
 */

#ifndef PJS_FRAME_PROCESSOR_H_
#define PJS_FRAME_PROCESSOR_H_

#include "project_common.h"
#include "project_global.h"
#include <pjmedia.h>
#include "pjs_utils.h"

class pjs_frame_processor
{
protected:
	pjs_global & m_global;
	pj_pool_t *m_pool;

	PPJ_SemaphoreLock *m_lock;
	pj_thread_t *m_thread;
	pjs_event m_event;
	pjmedia_circ_buf *m_buffer;
	bool m_exit;
	bool m_ready;
	bool m_started;
	unsigned m_samples_per_frame;

	static void* s_thread_proc(pjs_frame_processor *processor)
	{
		if (processor)
			return processor->thread_proc();
		else
			return NULL;
	}

	void* thread_proc();
	virtual void on_process_frame(const pjmedia_frame *frame)=0;
	void start();
	void stop();
public:
	pjs_frame_processor(pjs_global& global, unsigned capacity,
			unsigned samples_per_frame);

	virtual ~pjs_frame_processor();

	pj_status_t process_frame(const pjmedia_frame* frame);
};

#endif /* PJS_FRAME_PROCESSOR_H_ */

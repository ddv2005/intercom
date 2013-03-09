#include "pjs_frame_processor.h"


pjs_frame_processor::pjs_frame_processor(pjs_global& global,
		unsigned capacity, unsigned samples_per_frame) :
		m_global(global), m_samples_per_frame(samples_per_frame)
{
	m_started = false;
	m_thread = NULL;
	m_ready = false;
	m_exit = false;
	m_pool = pj_pool_create(m_global.get_pool_factory(), "pjs_frame_processor",
			128, 128, NULL);
	m_lock = new PPJ_SemaphoreLock(m_pool, NULL, 1, 1);
	pj_status_t status = pjmedia_circ_buf_create(m_pool, capacity, &m_buffer);
	if (status == PJ_SUCCESS)
	{
		status = pj_thread_create(m_pool, "frame processor thread",
				(pj_thread_proc*) &s_thread_proc, this, PJ_THREAD_DEFAULT_STACK_SIZE, PJ_THREAD_SUSPENDED,
				&m_thread);

		if (status != PJ_SUCCESS)
			m_thread = NULL;
		else
			m_ready = true;
	}
	else
		m_buffer = NULL;
}

pjs_frame_processor::~pjs_frame_processor()
{
	stop();
	if (m_lock)
		delete m_lock;

	if (m_pool)
		pj_pool_release(m_pool);
}

pj_status_t pjs_frame_processor::process_frame(
		const pjmedia_frame* frame)
{
	if (frame->type == PJMEDIA_FRAME_TYPE_AUDIO)
	{
		m_lock->acquire();
		pjmedia_circ_buf_write(m_buffer, (pj_int16_t*) (frame->buf),
				frame->size >> 1);
		m_event.signal();
		m_lock->release();
	}
	return PJ_SUCCESS;
}

void pjs_frame_processor::stop()
{
	if (m_thread&&m_started)
	{
		m_started = false;
		m_exit = true;
		m_event.signal();
		pj_thread_join(m_thread);
		pj_thread_destroy(m_thread);
		m_thread = NULL;
	}
}

void pjs_frame_processor::start()
{
	if (m_thread&&!m_started)
	{
		m_started = true;
		pj_thread_resume(m_thread);
	}
}


void* pjs_frame_processor::thread_proc()
{
	{
		pj_status_t status;
		const pj_uint16_t frame_size = m_samples_per_frame;
		pj_int16_t buffer[frame_size];
		set_max_relative_thread_priority(-25);
		while (!m_exit)
		{
			m_lock->acquire();
			unsigned size = pjmedia_circ_buf_get_len(m_buffer);
			bool b = size >= frame_size;
			if (b)
			{
				status = pjmedia_circ_buf_read(m_buffer, buffer, frame_size);
				m_lock->release();
				if (status == PJ_SUCCESS)
				{
					pjmedia_frame frame;
					frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
					frame.buf = buffer;
					frame.size = frame_size << 1;
					frame.timestamp.u64 = 0;
					on_process_frame(&frame);
				}
			}
			else
				m_lock->release();
			if (!b)
			{
				m_event.wait(1000);
			}
		}
		return 0;
	}
}

#include "pjs_audio_combine.h"

#define THIS_FILE   "pjs_audio_combine.cpp"

void pjs_audio_combine::media_sync(pjs_media_sync_param_t param)
{
	if (m_ready)
	{
		switch (param)
		{
			case PJMS_SYNC_BEFORE:
			{
				if (!m_oneway && m_putbuffer)
				{
					pjmedia_frame frame;
					frame.type = PJMEDIA_FRAME_TYPE_NONE;
					frame.buf = m_putbuffer;
					frame.size = m_buffer_size;
					get_frame(&frame);
					if (frame.type != PJMEDIA_FRAME_TYPE_NONE)
						pjmedia_port_put_frame(m_combiner, &frame);
				}
				break;
			}
			case PJMS_SYNC_AFTER:
			{
				pjmedia_frame frame;
				frame.type = PJMEDIA_FRAME_TYPE_NONE;
				frame.buf = m_getbuffer;
				frame.size = m_buffer_size;
				if (pjmedia_port_get_frame(m_combiner, &frame) == PJ_SUCCESS)
					put_frame(&frame);
				break;
			}
		}

	}
}

pjs_audio_combine::pjs_audio_combine(pjs_global &global, pjs_media_sync &sync,
		unsigned output_clock_rate, unsigned input_clock_rate,
		unsigned input_samples_per_frame, bool oneway) :
		m_global(global), m_sync(sync), m_combiner(NULL), m_input1(NULL), m_input2(
				NULL), m_resample1(NULL), m_resample2(NULL), m_oneway(oneway)
{
	m_clock_rate = output_clock_rate;
	m_started = false;
	m_ready = false;
	m_putbuffer = NULL;
	m_buffer_size = 0;
	if (input_clock_rate != output_clock_rate)
		m_samples_per_frame = 2 * input_samples_per_frame * output_clock_rate /input_clock_rate;
	else
		m_samples_per_frame = 2 * input_samples_per_frame;
	m_pool = pj_pool_create(m_global.get_pool_factory(), "pjs_audio_combine", 128,
			128, NULL);
	if (m_pool)
	{
		if (pjmedia_splitcomb_create(m_pool, output_clock_rate, 2,
				m_samples_per_frame, 16, 0, &m_combiner) == PJ_SUCCESS)
		{
			if (pjmedia_splitcomb_create_rev_channel(m_pool, m_combiner, 0, 0,
					&m_input1) == PJ_SUCCESS)
			{
				if (pjmedia_splitcomb_create_rev_channel(m_pool, m_combiner, 1, 0,
						&m_input2) == PJ_SUCCESS)
				{
					if (input_clock_rate != output_clock_rate)
					{
						if (pjmedia_resample_port_create(m_pool, m_input1,
								output_clock_rate,
								PJMEDIA_RESAMPLE_DONT_DESTROY_DN
										| PJMEDIA_RESAMPLE_USE_SMALL_FILTER,
								&m_resample1)==PJ_SUCCESS)
						{

							if (pjmedia_resample_port_create(m_pool, m_input2,
									output_clock_rate,
									PJMEDIA_RESAMPLE_DONT_DESTROY_DN
											| PJMEDIA_RESAMPLE_USE_SMALL_FILTER,
									&m_resample2)==PJ_SUCCESS)
							{
								m_ready = true;
							}
							else
								m_resample2 = NULL;
						}
						else
							m_resample1 = NULL;
					}
					else
						m_ready = true;
					if (m_ready)
					{
						m_buffer_size = m_combiner->info.samples_per_frame
								* m_combiner->info.channel_count
								* m_combiner->info.bits_per_sample / 8;
						m_getbuffer = (pj_int16_t*) pj_pool_alloc(m_pool, m_buffer_size);
						if (!m_oneway)
						{
							m_putbuffer = (pj_int16_t*) pj_pool_alloc(m_pool, m_buffer_size);
							m_ready = m_getbuffer && m_putbuffer;
						}
						else
						{
							m_ready = m_getbuffer;
							m_putbuffer = NULL;
						}
					}
				}
				else
					m_input2 = NULL;
			}
			else
				m_input1 = NULL;
		}
		else
			m_combiner = NULL;
	}
}

pjs_audio_combine::~pjs_audio_combine()
{
	stop();
	if (m_resample2)
		pjmedia_port_destroy(m_resample2);
	if (m_resample1)
		pjmedia_port_destroy(m_resample1);
	if (m_input1)
		pjmedia_port_destroy(m_input1);
	if (m_input2)
		pjmedia_port_destroy(m_input2);
	if (m_combiner)
		pjmedia_port_destroy(m_combiner);
	pj_pool_release(m_pool);
}

void pjs_audio_combine::start()
{
	if (m_ready && !m_started)
	{
		m_started = true;
		m_sync.add_callback(this);
	}
}

void pjs_audio_combine::stop()
{
	if (m_started)
	{
		m_started = false;
		m_sync.remove_callback(this);
	}
}


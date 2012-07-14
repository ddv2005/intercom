#include "pjs_port.h"

pj_status_t pjs_port::get_frame_s( pjmedia_port *port,
						    pjmedia_frame *frame )
{
	if(port && (port->port_data.pdata))
	{
		return ((pjs_port*)port->port_data.pdata)->get_frame(frame);
	}
	else
	{
		frame->type = PJMEDIA_FRAME_TYPE_NONE;
		return PJ_EINVALIDOP;
	}
}

pj_status_t pjs_port::put_frame_s( pjmedia_port *port,
						    const pjmedia_frame *frame )
{
	if(port && (port->port_data.pdata))
	{
		return ((pjs_port*)port->port_data.pdata)->put_frame(frame);
	}
	else
	{
		return PJ_EINVALIDOP;
	}
}

pj_status_t pjs_port::port_destroy_s( pjmedia_port *port )
{
	if(port && (port->port_data.pdata))
	{
		delete ((pjs_port*)port->port_data.pdata);
		port->port_data.pdata = NULL;
		return PJ_SUCCESS;
	}
	else
		return PJ_EINVALIDOP;
}

pj_status_t pjs_port::get_frame(pjmedia_frame *frame)
{
	frame->type = PJMEDIA_FRAME_TYPE_NONE;
	return PJ_EINVALIDOP;
}

pj_status_t pjs_port::put_frame(const pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(frame);
    return PJ_SUCCESS;
}

pjs_port::pjs_port(const char * name,unsigned signature,
			  unsigned sampling_rate,
		      unsigned channel_count,
		      unsigned samples_per_frame,
		      unsigned bits_per_sample,
		      unsigned options):m_options(options)
{
	m_rx_mute = m_tx_mute = false;

	if(name)
	{
		m_name = (char *)malloc(strlen(name)+1);
		strcpy(m_name,name);
	}
	else
		m_name = NULL;
	pj_str_t name_str = pj_str(m_name);
	memset(&m_port,0,sizeof(m_port));
    pjmedia_port_info_init(&m_port.info, &name_str, signature, sampling_rate,
			   channel_count, bits_per_sample, samples_per_frame);

    m_port.get_frame = &get_frame_s;
    m_port.put_frame = &put_frame_s;
    m_port.on_destroy = &port_destroy_s;
    m_port.port_data.pdata = this;
}

pjs_port::~pjs_port()
{
	if(m_name)
		free(m_name);

}




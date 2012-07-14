#ifndef PJS_PORT_H_
#define PJS_PORT_H_

#include "project_common.h"
#include <pjmedia.h>

#define PGP_RX_DISABLE	1
#define PGP_TX_DISABLE	2

class pjs_port
{
protected:
	char * 		m_name;
	unsigned    m_options;
	pjmedia_port m_port;

	bool		m_tx_mute;
	bool		m_rx_mute;

	static pj_status_t get_frame_s( pjmedia_port *port,
						    pjmedia_frame *frame );
	static pj_status_t put_frame_s( pjmedia_port *port,
						    const pjmedia_frame *frame );
    static pj_status_t port_destroy_s( pjmedia_port *port );

	virtual pj_status_t get_frame(pjmedia_frame *frame);
	virtual pj_status_t put_frame(const pjmedia_frame *frame);
public:
	pjs_port(const char * name,unsigned signature,
			  unsigned sampling_rate,
		      unsigned channel_count,
		      unsigned samples_per_frame,
		      unsigned bits_per_sample,
		      unsigned options);
	virtual ~pjs_port();
	pjmedia_port *get_mediaport() { return &m_port; }

	CREATE_CLASS_PROP_FUNCTIONS(bool, rx_mute)
	CREATE_CLASS_PROP_FUNCTIONS(bool, tx_mute)
};

template <class T>
class pjs_port_with_callback:public pjs_port
{
public:
	typedef pj_status_t  (T::*put_frame_callback)(const pjmedia_frame *frame);
	typedef pj_status_t  (T::*get_frame_callback)(pjmedia_frame *frame);
protected:
	T*				   m_callback_object;
	put_frame_callback m_put_frame_callback;
	get_frame_callback m_get_frame_callback;

	virtual pj_status_t get_frame(pjmedia_frame *frame)
	{
		if(m_get_frame_callback)
			return (m_callback_object->*m_get_frame_callback)(frame);
		else
		{
			frame->type = PJMEDIA_FRAME_TYPE_NONE;
			return PJ_EINVALIDOP;
		}
	}

	virtual pj_status_t put_frame(const pjmedia_frame *frame)
	{
		if(m_put_frame_callback)
			return (m_callback_object->*m_put_frame_callback)(frame);
		else
			return PJ_EINVALIDOP;
	}

public:
	pjs_port_with_callback(const char * name,unsigned signature,
				  unsigned sampling_rate,
			      unsigned channel_count,
			      unsigned samples_per_frame,
			      unsigned bits_per_sample,
			      unsigned options,
			      T* callback_object,
			      put_frame_callback put_frame_callback,
			      get_frame_callback get_frame_callback):pjs_port(name,signature,sampling_rate,channel_count,samples_per_frame,bits_per_sample,options),
			    	  m_callback_object(callback_object),m_put_frame_callback(put_frame_callback),m_get_frame_callback(get_frame_callback)
	{
	}
	virtual ~pjs_port_with_callback(){};
};




#endif /* PJS_PORT_H_ */

#ifndef WAPA_UVC_CAMERA_H_
#define WAPA_UVC_CAMERA_H_
#include <ptlib.h>
#include "wapa_camera.h"
#include "uvc/uvcvideo.h"

class WAPAUVCCamera: public WAPACamera
{
protected:
	PString m_device;
	PString m_device_params;
	extra_camera_params_t m_extra_params;
	input_t m_input;
	context_t m_context;
	char **   m_av;
	bool			m_ready;
	bool			m_started;
	static void input_callback_s(input_t *input,unsigned char *buf, int size,struct timeval timestamp)
	{
		if(input&&(input->user_data))
			((WAPAUVCCamera*)input->user_data)->input_callback(input,buf,size,timestamp);
	}
	void input_callback(input_t *input,unsigned char *buf, int size,struct timeval timestamp);
public:
	WAPAUVCCamera(WAPACameraCallback *callback,PString device, PString device_params, extra_camera_params_t &extra_params);
	virtual int Start();
	virtual void Stop();
	virtual ~WAPAUVCCamera();
};


#endif /* WAPA_UVC_CAMERA_H_ */

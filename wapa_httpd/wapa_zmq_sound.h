#ifndef WAPA_ZMQ_SOUND_H_
#define WAPA_ZMQ_SOUND_H_

#include <ptlib.h>
#include "wapa_camera.h"
#include "zeromq/include/zmq.h"
#include "zeromq/pjs_zeromq_common.h"

class WAPAZMQSound;
typedef PThreadFunction<WAPAZMQSound> PZMQSoundThread;
class WAPAZMQSound: public WAPACamera
{
protected:
	PString m_device;
	PString m_device_params;
	extra_camera_params_t m_extra_params;
	PZMQSoundThread *m_thread;

	void SoundThread(PZMQSoundThread *thread);
public:
	WAPAZMQSound(WAPACameraCallback *callback,PString device, PString device_params, extra_camera_params_t &extra_params);
	virtual int Start();
	virtual void Stop();
	virtual ~WAPAZMQSound();
};


#endif /* WAPA_ZMQ_SOUND_H_ */

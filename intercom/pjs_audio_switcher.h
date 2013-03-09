#ifndef PJS_AUDIO_SWITCHER_H_
#define PJS_AUDIO_SWITCHER_H_

class pjs_audio_switcher
{
public:
	virtual ~pjs_audio_switcher(){};
	virtual pj_status_t connect(pjsua_conf_port_id source,
			pjsua_conf_port_id sink)=0;
	virtual pj_status_t disconnect(pjsua_conf_port_id source,
			pjsua_conf_port_id sink)=0;
};

#endif /* PJS_AUDIO_SWITCHER_H_ */

#ifndef PJS_MEDIA_SYNC_H_
#define PJS_MEDIA_SYNC_H_

#include "project_common.h"
#include <pjmedia.h>
#include "pjs_utils.h"

#define PJMS_GET_BEFORE	1
#define PJMS_GET_AFTER	2
#define PJMS_PUT_BEFORE	3
#define PJMS_PUT_AFTER	4
#define PJMS_SYNC_BEFORE	PJMS_GET_BEFORE
#define PJMS_SYNC_AFTER		PJMS_GET_AFTER


typedef pj_int32_t pjs_media_sync_param_t;

class pjs_media_sync_callback
{
public:
	virtual ~pjs_media_sync_callback()
	{
	}

	virtual void media_sync(pjs_media_sync_param_t param)=0;
};

class pjs_media_sync: public pjs_callback_class<pjs_media_sync_callback,
		pjs_media_sync_param_t>
{
protected:
	virtual void on_callback(pjs_media_sync_callback* cbt,
			pjs_media_sync_param_t user_data)
	{
		cbt->media_sync(user_data);
	}
public:
	virtual ~pjs_media_sync()
	{
	}

	virtual void media_sync(pjs_media_sync_param_t param)
	{
		fire_callbacks(param);
	}
};

#endif /* PJS_MEDIA_SYNC_H_ */

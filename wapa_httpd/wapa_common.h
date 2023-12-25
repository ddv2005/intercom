#ifndef WAPA_COMMON_H
#define WAPA_COMMON_H

using namespace std;

#ifdef _WIN32
#define __WINDOWS
#define __MSVC
#undef	__LINUX
#undef	__GCC
#else
#undef	__WINDOWS
#undef	__MSVC
#define	__LINUX
#define __GCC
#endif

#define WR_OK		0
#define WR_ERROR	-1

#define DEBUG_LEVEL	6
#define ERROR_LEVEL	0

// Camera Type
#define CT_MPEG4_WAPA	0
#define CT_JPEG_GST		1
#define CT_UVC				2
#define CT_SOUND_ZMQ	3

//Frame Type
#define MPEG4_HD_TYPE		0
#define MPEG4_CIF_TYPE		2
#define JPEG_TYPE			10
#define SND_MULAW_TYPE		11
#define SND_ALAW_TYPE			12

typedef struct extra_camera_params_t
{
	BYTE		m_type;
}extra_camera_params_t;

#endif

#include "wapa_uvc_camera.h"

#include <stdio.h>

static int setargs(char *args, char **argv)
{
	int count = 0;

	while (isspace(*args))
		++args;
	while (*args)
	{
		if (argv)
			argv[count] = args;
		while (*args && !isspace(*args))
			++args;
		if (argv && *args)
			*args++ = '\0';
		while (isspace(*args))
			++args;
		count++;
	}
	return count;
}

char **parsedargs(const char *args_, int *argc)
{
	char * args = NULL;;
	char **argv = NULL;
	int argn = 0;

	if (args_ && *args_ && (args = strdup(args_)) && (argn = setargs(args, NULL))
			&& (argv = (char**)malloc((argn + 1) * sizeof(char *))))
	{
		*argv++ = args;
		argn = setargs(args, argv);
	}

	if (args && !argv)
		free(args);

	*argc = argn;
	return argv;
}

void freeparsedargs(char **argv)
{
	if (argv)
	{
		free(argv[-1]);
		free(argv - 1);
	}
}
WAPAUVCCamera::WAPAUVCCamera(WAPACameraCallback *callback, PString device,
		PString device_params, extra_camera_params_t &extra_params) :
		WAPACamera(callback), m_device(device), m_device_params(device_params)
{
	memset(&m_input, 0, sizeof(m_input));
	memset(&m_context, 0, sizeof(m_context));
	m_input.user_data = this;
	m_input.callback = input_callback_s;
	m_context.input = &m_input;

	m_av = parsedargs((const char*)m_device_params,&m_input.param.argc);
	if(m_av)
	{
		m_input.param.argc++;
		m_input.param.argv[0] = m_av[-1];
		for(int i=1; i<m_input.param.argc; i++)
			m_input.param.argv[i] = m_av[i-1];
	}
	else
		m_input.param.argc = 0;
	m_ready = input_init(m_device, &m_context)==0;
	m_started = false;
}

void WAPAUVCCamera::input_callback(input_t *input, unsigned char *buf, int size,
		struct timeval timestamp)
{
	if (m_callback&&input&&size&&buf)
	try
	{
		if(input->in_formats[input->currentFormat].format.pixelformat==V4L2_PIX_FMT_MJPEG)
		{
			m_callback->OnVideoFrame(
					this, JPEG_TYPE,
					m_context.videoIn->width, m_context.videoIn->height,
					buf, size,0);
		}
	}
	catch (...)
	{
		PTRACE( 1,
				"OnVideoFrame error");
	}
}

int WAPAUVCCamera::Start()
{
	if(m_ready&&!m_started)
		m_started = input_run(&m_context)==0;
	return m_started;
}

void WAPAUVCCamera::Stop()
{
	if(m_started)
	{
		input_stop(&m_context);
		m_started = false;
	}
}

WAPAUVCCamera::~WAPAUVCCamera()
{
	freeparsedargs(m_av);
}

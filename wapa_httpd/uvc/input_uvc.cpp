/*******************************************************************************
 # Linux-UVC streaming input-plugin for MJPG-streamer                           #
 #                                                                              #
 # This package work with the Logitech UVC based webcams with the mjpeg feature #
 #                                                                              #
 # Copyright (C) 2005 2006 Laurent Pinchart &&  Michel Xhaard                   #
 #                    2007 Lucas van Staden                                     #
 #                    2007 Tom Stöveken                                         #
 #                                                                              #
 # This program is free software; you can redistribute it and/or modify         #
 # it under the terms of the GNU General Public License as published by         #
 # the Free Software Foundation; either version 2 of the License, or            #
 # (at your option) any later version.                                          #
 #                                                                              #
 # This program is distributed in the hope that it will be useful,              #
 # but WITHOUT ANY WARRANTY; without even the implied warranty of               #
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
 # GNU General Public License for more details.                                 #
 #                                                                              #
 # You should have received a copy of the GNU General Public License            #
 # along with this program; if not, write to the Free Software                  #
 # Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
 #                                                                              #
 *******************************************************************************/

#include <ptlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pthread.h>

#include "utils.h"
#include "v4l2uvc.h" // this header will includes the ../../mjpg_streamer.h
/*
 * UVC resolutions mentioned at: (at least for some webcams)
 * http://www.quickcamteam.net/hcl/frame-format-matrix/
 */
static const struct
{
	const char *string;
	const int width, height;
} resolutions[] =
{
{ "QSIF", 160, 120 },
{ "QCIF", 176, 144 },
{ "CGA", 320, 200 },
{ "QVGA", 320, 240 },
{ "CIF", 352, 288 },
{ "VGA", 640, 480 },
{ "SVGA", 800, 600 },
{ "XGA", 1024, 768 },
{ "SXGA", 1280, 1024 } };

void *cam_thread(void *);
void cam_cleanup(context_t *pcontext);

/*** plugin interface functions ***/
/******************************************************************************
 Description.: This function ializes the plugin. It parses the commandline-
 parameter and stores the default and parsed values in the
 appropriate variables.
 Input Value.: param contains among others the command-line string
 Return Value: 0 if everything is fine
 1 if "--help" was triggered, in this case the calling programm
 should stop running and leave.
 ******************************************************************************/
int input_init(const char * dev, context_t *context)
{
	input_parameter *param = &context->input->param;
	context->stop = 0;
	context->cleaned = 0;
	input_t *input = context->input;
	char *s;
	int gquality = 80, minimum_size = 10;
	int width = 640, height = 480, fps = 5, fps_div = 1, format =
			V4L2_PIX_FMT_MJPEG, i;

	/* parse the parameters */
	reset_getopt();
	while (1)
	{
		int option_index = 0, c = 0;
		static struct option long_options[] =
		{
		{ "r", required_argument, 0, 0 },
		{ "resolution", required_argument, 0, 0 },
		{ "f", required_argument, 0, 0 },
		{ "fps", required_argument, 0, 0 },
		{ "y", no_argument, 0, 0 },
		{ "yuv", no_argument, 0, 0 },
		{ "q", required_argument, 0, 0 },
		{ "quality", required_argument, 0, 0 },
		{ "m", required_argument, 0, 0 },
		{ "minimum_size", required_argument, 0, 0 },
		{ "s", required_argument, 0, 0 },
		{ "fpsdiv", required_argument, 0, 0 },
		{ 0, 0, 0, 0 } };

		/* parsing all parameters according to the list above is sufficent */
		c = getopt_long_only(param->argc, param->argv, "", long_options,
				&option_index);

		/* no more options to parse */
		if (c == -1)
			break;

		/* dispatch the given options */
		switch (option_index)
		{
			/* r, resolution */
			case 0:
			case 1:
				width = -1;
				height = -1;

				/* try to find the resolution in lookup table "resolutions" */
				for (i = 0; i < LENGTH_OF(resolutions); i++)
				{
					if (strcmp(resolutions[i].string, optarg) == 0)
					{
						width = resolutions[i].width;
						height = resolutions[i].height;
					}
					break;
				}
				/* done if width and height were set */
				if (width != -1 && height != -1)
					break;
				/* parse value as decimal value */
				width = strtol(optarg, &s, 10);
				height = strtol(s + 1, NULL, 10);
				break;

				/* f, fps */
			case 2:
			case 3:
				fps = atoi(optarg);
				break;

				/* y, yuv */
			case 4:
			case 5:
				format = V4L2_PIX_FMT_YUYV;
				break;

				/* q, quality */
			case 6:
			case 7:
				format = V4L2_PIX_FMT_YUYV;
				gquality = MIN(MAX(atoi(optarg), 0), 100);
				break;

				/* m, minimum_size */
			case 8:
			case 9:
				minimum_size = MAX(atoi(optarg), 0);
				break;

			case 10:
			case 11:
				fps_div = atoi(optarg);
				if (fps_div < 1)
					fps_div = 1;
				break;

			default:
				return -2;
		}
	}
	/* allocate webcam datastructure */
	context->videoIn = (vdIn*) malloc(sizeof(struct vdIn));
	if (context->videoIn == NULL)
	{
		PTRACE(0, "not enough memory for videoIn");
		return -2;
	}
	memset(context->videoIn, 0, sizeof(struct vdIn));
	input->frame_interval = 1000 * fps_div / fps;

	/* display the parsed values */
	PTRACE(2, "Using V4L2 device.: "<< dev);
	PTRACE(2, "Desired Resolution: "<<width<<" x "<<height);
	PTRACE(2, "Frames Per Second.: "<<fps<<"/"<<fps_div);
	PTRACE(2,
			"Format............: "<< ((format == V4L2_PIX_FMT_YUYV) ? "YUV" : "MJPEG"));
	if (format == V4L2_PIX_FMT_YUYV)
		PTRACE(2, "JPEG Quality......: "<< gquality);

	context->minimum_size = minimum_size;
	/* open video device and prepare data structure */
	if (init_videoIn(context->videoIn, dev, width, height, fps, fps_div, format,
			1, input) < 0)
	{
		PTRACE(0, "init_VideoIn failed");
		return -3;
	}
	return 0;
}

/******************************************************************************
 Description.: Stops the execution of worker thread
 Input Value.: -
 Return Value: always 0
 ******************************************************************************/
int input_stop(context_t *context)
{
	void *result;
	PTRACE(2, "will cancel camera thread");
	context->stop = 1;
	pthread_cancel(context->threadID);
	pthread_join(context->threadID, &result);
	cam_cleanup(context);
	return 0;
}

/******************************************************************************
 Description.: spins of a worker thread
 Input Value.: -
 Return Value: always 0
 ******************************************************************************/
int input_run(context_t *context)
{
	input_t *input = context->input;
	/* create thread and pass context to thread function */
	pthread_create(&(context->threadID), NULL, cam_thread, context);
	pthread_detach(context->threadID);
	return 0;
}

/******************************************************************************
 Description.: this thread worker grabs a frame and copies it to the global buffer
 Input Value.: unused
 Return Value: unused, always NULL
 ******************************************************************************/
void *cam_thread(void *arg)
{

	context_t *pcontext = (context_t *) arg;
	input_t *input = pcontext->input;

	while (!pcontext->stop)
	{
		while (!pcontext->stop
				&& pcontext->videoIn->streamingState == STREAMING_PAUSED)
		{
			usleep(100); // maybe not the best way so FIXME
		}
		if (pcontext->stop)
			break;

		/* grab a frame */
		if (uvcGrab(pcontext->videoIn) < 0)
		{
			PTRACE(0, "Error grabbing frames");
			return NULL;
		}

		/*
		 * Workaround for broken, corrupted frames:
		 * Under low light conditions corrupted frames may get captured.
		 * The good thing is such frames are quite small compared to the regular pictures.
		 * For example a VGA (640x480) webcam picture is normally >= 8kByte large,
		 * corrupted frames are smaller.
		 */
		if (pcontext->videoIn->buf.bytesused < pcontext->minimum_size)
		{
			PTRACE(3, "dropping too small frame, assuming it as broken");
			continue;
		}

		if (input->callback)
			input->callback(input, pcontext->videoIn->tmpbuffer,
					pcontext->videoIn->buf.bytesused, pcontext->videoIn->buf.timestamp);

		/* only use usleep if the fps is below 5, otherwise the overhead is too long */
		if (pcontext->videoIn->fps < 60)
		{
			usleep(
					1000 * 1000 * pcontext->videoIn->fps_div / pcontext->videoIn->fps
							- 5000);
		}
		else
		{
		}
	}

	PTRACE(1, "leaving input thread");

	return NULL;
}

/******************************************************************************
 Description.:
 Input Value.:
 Return Value:
 ******************************************************************************/
void cam_cleanup(context_t *pcontext)
{
	if (!pcontext->cleaned)
	{
		pcontext->cleaned = 1;

		close_v4l2(pcontext->videoIn);
		if (pcontext->videoIn->tmpbuffer != NULL)
			free(pcontext->videoIn->tmpbuffer);
		if (pcontext->videoIn != NULL)
			free(pcontext->videoIn);
	}
}

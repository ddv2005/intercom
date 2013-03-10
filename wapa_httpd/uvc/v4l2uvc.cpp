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

#include <stdlib.h>
#include <errno.h>
#include "v4l2uvc.h"
#include <ptlib.h>

/* ioctl with a number of retries in the case of failure
 * args:
 * fd - device descriptor
 * IOCTL_X - ioctl reference
 * arg - pointer to ioctl data
 * returns - ioctl result
 */
int xioctl(int fd, int IOCTL_X, void *arg)
{
	int ret = 0;
	int tries = IOCTL_RETRY;
	do
	{
		ret = IOCTL_VIDEO(fd, IOCTL_X, arg);
	} while (ret && tries--
			&& ((errno == EINTR) || (errno == EAGAIN) || (errno == ETIMEDOUT)));

	if (ret && (tries <= 0))
	{
		PTRACE(0,
				"ioctl ("<<IOCTL_X<<") retried "<< IOCTL_RETRY <<" times - giving up: "<< strerror(errno) <<")");
	}

	return (ret);
}

static int init_v4l2(struct vdIn *vd);

int init_videoIn(struct vdIn *vd, const char *device, int width, int height, int fps,
		int fps_div, int format, int grabmethod, input_t * input)
{
	if (vd == NULL || device == NULL)
		return -1;
	if (width == 0 || height == 0)
		return -1;
	if (grabmethod < 0 || grabmethod > 1)
		grabmethod = 1; //mmap by default;
	vd->videodevice = NULL;
	vd->status = NULL;
	vd->pictName = NULL;
	vd->videodevice = (char *) calloc(1, 16 * sizeof(char));
	vd->status = (char *) calloc(1, 100 * sizeof(char));
	vd->pictName = (char *) calloc(1, 80 * sizeof(char));
	snprintf(vd->videodevice, 12, "%s", device);
	vd->toggleAvi = 0;
	vd->getPict = 0;
	vd->signalquit = 1;
	vd->width = width;
	vd->height = height;
	vd->fps = fps;
	vd->fps_div = fps_div;
	vd->formatIn = format;
	vd->grabmethod = grabmethod;
	if (init_v4l2(vd) < 0)
	{
		PTRACE(0, "Init v4L2 failed !! exit fatal");
		goto error;;
	}

	// enumerating formats
	struct v4l2_format currentFormat;
	currentFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	xioctl(vd->fd, VIDIOC_G_FMT, &currentFormat);

	input->in_formats = NULL;
	for (input->formatCount = 0; 1; input->formatCount++)
	{
		struct v4l2_fmtdesc fmtdesc;
		fmtdesc.index = input->formatCount;
		fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (xioctl(vd->fd, VIDIOC_ENUM_FMT, &fmtdesc) < 0)
		{
			break;
		}

		if (input->in_formats == NULL)
		{
			input->in_formats = (input_format*) calloc(1, sizeof(input_format));
		}
		else
		{
			input->in_formats = (input_format*) realloc(input->in_formats,
					(input->formatCount + 1) * sizeof(input_format));
		}

		if (input->in_formats == NULL)
		{
			PTRACE(0, "Calloc/realloc failed: "<<strerror(errno));
			return -1;
		}

		memcpy(&input->in_formats[input->formatCount], &fmtdesc,
				sizeof(input_format));

		if (fmtdesc.pixelformat == format)
			input->currentFormat = input->formatCount;

		struct v4l2_frmsizeenum fsenum;
		fsenum.index = input->formatCount;
		fsenum.pixel_format = fmtdesc.pixelformat;
		int j = 0;
		input->in_formats[input->formatCount].supportedResolutions = NULL;
		input->in_formats[input->formatCount].resolutionCount = 0;
		input->in_formats[input->formatCount].currentResolution = -1;
		while (1)
		{
			fsenum.index = j;
			j++;
			if (xioctl(vd->fd, VIDIOC_ENUM_FRAMESIZES, &fsenum) == 0)
			{
				input->in_formats[input->formatCount].resolutionCount++;
				if (input->in_formats[input->formatCount].supportedResolutions == NULL)
				{
					input->in_formats[input->formatCount].supportedResolutions =
							(input_resolution*) calloc(1, sizeof(input_resolution));
				}
				else
				{
					input->in_formats[input->formatCount].supportedResolutions =
							(input_resolution*) realloc(
									input->in_formats[input->formatCount].supportedResolutions,
									j * sizeof(input_resolution));
				}

				if (input->in_formats[input->formatCount].supportedResolutions == NULL)
				{
					PTRACE(0, "Calloc/realloc failed");
					return -1;
				}

				input->in_formats[input->formatCount].supportedResolutions[j - 1].width =
						fsenum.discrete.width;
				input->in_formats[input->formatCount].supportedResolutions[j - 1].height =
						fsenum.discrete.height;
				if (format == fmtdesc.pixelformat)
				{
					input->in_formats[input->formatCount].currentResolution = (j - 1);
				}
				else
				{
				}
			}
			else
			{
				break;
			}
		}
	}

	/* alloc a temp buffer to reconstruct the pict */
	vd->framesizeIn = (vd->width * vd->height << 1);
	switch (vd->formatIn)
	{
		case V4L2_PIX_FMT_MJPEG:
			vd->tmpbuffer = (unsigned char *) calloc(1, (size_t) vd->framesizeIn);
			if (!vd->tmpbuffer)
				goto error;
			vd->framebuffer = (unsigned char *) calloc(1,
					(size_t) vd->width * (vd->height + 8) * 2);
			break;
		case V4L2_PIX_FMT_YUYV:
			vd->framebuffer = (unsigned char *) calloc(1, (size_t) vd->framesizeIn);
			break;
		default:
			PTRACE(0, "No pixel format");
			goto error;
			break;

	}

	if (!vd->framebuffer)
		goto error;
	return 0;
	error: free(vd->videodevice);
	free(vd->status);
	free(vd->pictName);
	CLOSE_VIDEO(vd->fd);
	return -1;
}

static int init_v4l2(struct vdIn *vd)
{
	int i;
	int ret = 0;
	if ((vd->fd = OPEN_VIDEO(vd->videodevice, O_RDWR)) == -1)
	{
		PTRACE(0, "ERROR opening V4L interface");
		return -1;
	}

	memset(&vd->cap, 0, sizeof(struct v4l2_capability));
	ret = xioctl(vd->fd, VIDIOC_QUERYCAP, &vd->cap);
	if (ret < 0)
	{
		PTRACE(0,
				"Error opening device "<< vd->videodevice <<": unable to query device.");
		goto fatal;
	}

	if ((vd->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0)
	{
		PTRACE(0,
				"Error opening device "<< vd->videodevice <<": video capture not supported.");
		goto fatal;;
	}

	if (vd->grabmethod)
	{
		if (!(vd->cap.capabilities & V4L2_CAP_STREAMING))
		{
			PTRACE(0, vd->videodevice<<" does not support streaming i/o");
			goto fatal;
		}
	}
	else
	{
		if (!(vd->cap.capabilities & V4L2_CAP_READWRITE))
		{
			PTRACE(0, vd->videodevice<<" does not support read i/o");
			goto fatal;
		}
	}

	/*
	 * set format in
	 */
	memset(&vd->fmt, 0, sizeof(struct v4l2_format));
	vd->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vd->fmt.fmt.pix.width = vd->width;
	vd->fmt.fmt.pix.height = vd->height;
	vd->fmt.fmt.pix.pixelformat = vd->formatIn;
	vd->fmt.fmt.pix.field = V4L2_FIELD_ANY;
	ret = xioctl(vd->fd, VIDIOC_S_FMT, &vd->fmt);
	if (ret < 0)
	{
		PTRACE(0,
				"Unable to set format: "<<vd->formatIn<<" res: "<<vd->width<<"x"<<vd->height<<"; strerr:"<<strerror(errno));
		goto fatal;
	}

	if ((vd->fmt.fmt.pix.width != vd->width)
			|| (vd->fmt.fmt.pix.height != vd->height))
	{
		vd->width = vd->fmt.fmt.pix.width;
		vd->height = vd->fmt.fmt.pix.height;
		/*
		 * look the format is not part of the deal ???
		 */
		if (vd->formatIn != vd->fmt.fmt.pix.pixelformat)
		{
			if (vd->formatIn == V4L2_PIX_FMT_MJPEG)
			{
				PTRACE(0,
						"The inpout device does not supports MJPEG mode\nYou may also try the YUV mode (-yuv option), but it requires a much more CPU power");
				goto fatal;
			}
			else if (vd->formatIn == V4L2_PIX_FMT_YUYV)
			{
				PTRACE(0, "The input device does not supports YUV mode");
				goto fatal;
			}
		}
		else
		{
			vd->formatIn = vd->fmt.fmt.pix.pixelformat;
		}
	}

	/*
	 * set framerate
	 */
	struct v4l2_streamparm *setfps;
	setfps = (struct v4l2_streamparm *) calloc(1, sizeof(struct v4l2_streamparm));
	memset(setfps, 0, sizeof(struct v4l2_streamparm));
	setfps->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	setfps->parm.capture.timeperframe.numerator = vd->fps_div;
	setfps->parm.capture.timeperframe.denominator = vd->fps;
	ret = xioctl(vd->fd, VIDIOC_S_PARM, setfps);

	/*
	 * request buffers
	 */
	memset(&vd->rb, 0, sizeof(struct v4l2_requestbuffers));
	vd->rb.count = NB_BUFFER;
	vd->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vd->rb.memory = V4L2_MEMORY_MMAP;

	ret = xioctl(vd->fd, VIDIOC_REQBUFS, &vd->rb);
	if (ret < 0)
	{
		PTRACE(0, "Unable to allocate buffers");
		goto fatal;
	}

	/*
	 * map the buffers
	 */
	for (i = 0; i < NB_BUFFER; i++)
	{
		memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
		vd->buf.index = i;
		vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vd->buf.memory = V4L2_MEMORY_MMAP;
		ret = xioctl(vd->fd, VIDIOC_QUERYBUF, &vd->buf);
		if (ret < 0)
		{
			PTRACE(0, "Unable to query buffer");
			goto fatal;
		}

		vd->mem[i] = mmap(0 /* start anywhere */, vd->buf.length,
				PROT_READ | PROT_WRITE, MAP_SHARED, vd->fd, vd->buf.m.offset);
		if (vd->mem[i] == MAP_FAILED )
		{
			PTRACE(0, "Unable to map buffer");
			goto fatal;
		}
	}

	/*
	 * Queue the buffers.
	 */
	for (i = 0; i < NB_BUFFER; ++i)
	{
		memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
		vd->buf.index = i;
		vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vd->buf.memory = V4L2_MEMORY_MMAP;
		ret = xioctl(vd->fd, VIDIOC_QBUF, &vd->buf);
		if (ret < 0)
		{
			PTRACE(0, "Unable to queue buffer");
			goto fatal;;
		}
	}
	return 0;
	fatal: return -1;

}

static int video_enable(struct vdIn *vd)
{
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int ret;

	ret = xioctl(vd->fd, VIDIOC_STREAMON, &type);
	if (ret < 0)
	{
		PTRACE(0, "Unable to start capture");
		return ret;
	}
	vd->streamingState = STREAMING_ON;
	return 0;
}

static int video_disable(struct vdIn *vd, streaming_state disabledState)
{
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int ret;
	PTRACE(2, "Stopping capture\n");
	ret = xioctl(vd->fd, VIDIOC_STREAMOFF, &type);
	if (ret != 0)
	{
		PTRACE(0, "Unable to stop capture");
		return ret;
	}
	PTRACE(2, "Stopping capture done\n");
	vd->streamingState = disabledState;
	return 0;
}

int uvcGrab(struct vdIn *vd)
{
#define HEADERFRAME1 0xaf
	int ret;

	if (vd->streamingState == STREAMING_OFF)
	{
		if (video_enable(vd))
			goto err;
	}
	memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
	vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vd->buf.memory = V4L2_MEMORY_MMAP;

	ret = xioctl(vd->fd, VIDIOC_DQBUF, &vd->buf);
	if (ret < 0)
	{
		PTRACE(0, "Unable to dequeue buffer");
		goto err;
	}

	switch (vd->formatIn)
	{
		case V4L2_PIX_FMT_MJPEG:
			if (vd->buf.bytesused <= HEADERFRAME1)
			{
				/* Prevent crash
				 * on empty image */
				PTRACE(3, "Ignoring empty buffer ...");
				return 0;
			}

			/* memcpy(vd->tmpbuffer, vd->mem[vd->buf.index], vd->buf.bytesused);

			 memcpy (vd->tmpbuffer, vd->mem[vd->buf.index], HEADERFRAME1);
			 memcpy (vd->tmpbuffer + HEADERFRAME1, dht_data, sizeof(dht_data));
			 memcpy (vd->tmpbuffer + HEADERFRAME1 + sizeof(dht_data), vd->mem[vd->buf.index] + HEADERFRAME1, (vd->buf.bytesused - HEADERFRAME1));
			 */

			memcpy(vd->tmpbuffer, vd->mem[vd->buf.index], vd->buf.bytesused);

			break;

		case V4L2_PIX_FMT_YUYV:
			if (vd->buf.bytesused > vd->framesizeIn)
				memcpy(vd->framebuffer, vd->mem[vd->buf.index],
						(size_t) vd->framesizeIn);
			else
				memcpy(vd->framebuffer, vd->mem[vd->buf.index],
						(size_t) vd->buf.bytesused);
			break;

		default:
			goto err;
			break;
	}

	ret = xioctl(vd->fd, VIDIOC_QBUF, &vd->buf);
	if (ret < 0)
	{
		PTRACE(0, "Unable to requeue buffer");
		goto err;
	}

	return 0;

	err: vd->signalquit = 0;
	return -1;
}

int close_v4l2(struct vdIn *vd)
{
	if (vd->streamingState == STREAMING_ON)
		video_disable(vd, STREAMING_OFF);
	for (int i = 0; i < NB_BUFFER; i++)
		munmap(vd->mem[i], vd->buf.length);

	CLOSE_VIDEO(vd->fd);
	if (vd->tmpbuffer)
		free(vd->tmpbuffer);
	vd->tmpbuffer = NULL;
	free(vd->framebuffer);
	vd->framebuffer = NULL;
	free(vd->videodevice);
	free(vd->status);
	free(vd->pictName);
	vd->videodevice = NULL;
	vd->status = NULL;
	vd->pictName = NULL;
	return 0;
}

/*  It should set the capture resolution
 Cheated from the openCV cap_libv4l.cpp the method is the following:
 Turn off the stream (video_disable)
 Unmap buffers
 Close the filedescriptor
 Initialize the camera again with the new resolution
 */
int setResolution(struct vdIn *vd, int width, int height)
{
	int ret;
	vd->streamingState = STREAMING_PAUSED;
	if (video_disable(vd, STREAMING_PAUSED) == 0)
	{ // do streamoff
		int i;
		for (i = 0; i < NB_BUFFER; i++)
			munmap(vd->mem[i], vd->buf.length);

		if (CLOSE_VIDEO(vd->fd) == 0)
		{
			PTRACE(4, "Device closed successfully");
		}

		vd->width = width;
		vd->height = height;
		if (init_v4l2(vd) < 0)
		{
			PTRACE(0, " Init v4L2 failed !! exit fatal");
			return -1;
		}
		else
		{
			video_enable(vd);
			return 0;
		}
	}
	else
	{
		PTRACE(0, "Unable to disable streaming");
		return -1;
	}
	return ret;
}


/*******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2007 Tom Stöveken                                         #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
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

#define MAX_PLUGIN_ARGUMENTS	20

/* parameters for input plugin */
typedef struct _input_parameter input_parameter;
struct _input_parameter {
    int argc;
    char *argv[MAX_PLUGIN_ARGUMENTS];
};

typedef struct _input_resolution input_resolution;
struct _input_resolution {
    unsigned int width;
    unsigned int height;
};

typedef struct _input_format input_format;
struct _input_format {
    struct v4l2_fmtdesc format;
    input_resolution *supportedResolutions;
    int resolutionCount;
    char currentResolution;
};

typedef struct _input input_t;
typedef void (*input_callback_t) (input_t *input,unsigned char *buf, int size,struct timeval timestamp);
/* structure to store variables/functions for input plugin */
struct _input {
    input_parameter param; // this holds the command line arguments

    unsigned frame_interval;
    struct v4l2_jpegcompression jpegcomp;

    input_format *in_formats;
    int formatCount;
    int currentFormat; // holds the current format number
    input_callback_t callback;
    void *user_data;
};

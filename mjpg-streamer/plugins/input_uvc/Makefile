###############################################################
#
# Purpose: Makefile for "M-JPEG Streamer"
# Author.: Tom Stoeveken (TST)
# Version: 0.3
# License: GPL
#
###############################################################

OTHER_HEADERS = ../../mjpg_streamer.h ../../utils.h ../output.h ../input.h

CFLAGS += -DLINUX -D_GNU_SOURCE -Wall -shared -fPIC

ifeq ($(USE_LIBV4L2),true)
LFLAGS += -lv4l2
CFLAGS += -DUSE_LIBV4L2
endif

LFLAGS +=

all: input_uvc.so

clean:
	rm -f *.a *.o core *~ *.so *.lo

input_uvc.so: $(OTHER_HEADERS) input_uvc.c v4l2uvc.lo dynctrl.lo
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ input_uvc.c v4l2uvc.lo dynctrl.lo

v4l2uvc.lo: huffman.h uvc_compat.h v4l2uvc.c v4l2uvc.h
	$(CC) -c $(CFLAGS) -o $@ v4l2uvc.c

dynctrl.lo: dynctrl.c dynctrl.h
	$(CC) -c $(CFLAGS) -o $@ dynctrl.c

###############################################################
#
# Purpose: Makefile for "M-JPEG Streamer"
# Author.: Tom Stoeveken (TST)
# Version: 0.4
# License: GPL
#
###############################################################

# specifies where to install the binaries after compilation
# to use another directory you can specify it with:
# $ sudo make DESTDIR=/some/path install
# set the compiler to use
#export CC=/opt/gcc-4.7/bin/arm-linux-gnueabi-gcc

#export CFLAGS = -O3 -DLINUX -D_GNU_SOURCE -Wall -fomit-frame-pointer -mfloat-abi=softfp -mfpu=neon -mcpu=cortex-a9 -ftree-vectorize -fmessage-length=0 -marm -flto


# we are using the libraries "libpthread" and "libdl"
# libpthread is used to run several tasks (virtually) in parallel
# libdl is used to load the plugins (shared objects) at runtime
LFLAGS += -lpthread -ldl

# define the name of the program
APP_BINARY = mjpg_streamer

# define the names and targets of the plugins
PLUGINS = input_uvc.so
PLUGINS += output_udp.so
PLUGINS += output_http.so
PLUGINS += output_rtsp.so

# define the names of object files
OBJECTS=mjpg_streamer.o utils.o

# this is the first target, thus it will be used implictely if no other target
# was given. It defines that it is dependent on the application target and
# the plugins
all: application plugins

application: $(APP_BINARY)

plugins: $(PLUGINS)

$(APP_BINARY): mjpg_streamer.c mjpg_streamer.h mjpg_streamer.o utils.c utils.h utils.o
	$(CC) $(CFLAGS) $(OBJECTS) $(LFLAGS) -o $(APP_BINARY)
	chmod 755 $(APP_BINARY)

ifeq ($(USE_LIBV4L2),true)
input_uvc.so: mjpg_streamer.h utils.h
	make -C plugins/input_uvc USE_LIBV4L2=true all
	cp plugins/input_uvc/input_uvc.so .
else
input_uvc.so: mjpg_streamer.h utils.h
	make -C plugins/input_uvc all
	cp plugins/input_uvc/input_uvc.so .
endif

ifeq ($(WXP_COMPAT),true)
output_http.so: mjpg_streamer.h utils.h
	make -C plugins/output_http -DWXP_COMPAT all
	cp plugins/output_http/output_http.so .
else
output_http.so: mjpg_streamer.h utils.h
	make -C plugins/output_http all
	cp plugins/output_http/output_http.so .
endif

output_udp.so: mjpg_streamer.h utils.h
	make -C plugins/output_udp all
	cp plugins/output_udp/output_udp.so .

output_rtsp.so: mjpg_streamer.h utils.h
	make -C plugins/output_rtsp all
	cp plugins/output_rtsp/output_rtsp.so .
	

#input_http.so: mjpg_streamer.h utils.h
#	make -C plugins/input_http all
#	cp plugins/input_http/input_http.so .

# The viewer plugin requires the SDL library for compilation
# This is very uncommmon on embedded devices, so it is commented out and will
# not be build automatically. If you compile for PC, install libsdl and then
# execute the following command:
# make output_viewer.so
output_viewer.so: mjpg_streamer.h utils.h
	make -C plugins/output_viewer all
	cp plugins/output_viewer/output_viewer.so .

# cleanup
clean:
	make -C plugins/input_uvc $@
	make -C plugins/output_http $@
	make -C plugins/output_udp $@
	make -C plugins/output_rtsp $@
#	make -C plugins/input_http $@
	rm -f *.a *.o $(APP_BINARY) core *~ *.so *.lo

# useful to make a backup "make tgz"
tgz: clean
	mkdir -p backups
	tar czvf ./backups/mjpg_streamer_`date +"%Y_%m_%d_%H.%M.%S"`.tgz --exclude backups --exclude .svn *


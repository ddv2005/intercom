APP_DIR=$(CURDIR)/..
APP_HEADERS=${APP_DIR}/project_config.h ${APP_DIR}/pjs_utils.h \
${APP_DIR}/pjs_messages_queue.h ${APP_DIR}/pjs_intercom_messages.h \
${APP_DIR}/pjs_external_controller.h ${APP_DIR}/pjs_intercom_script_interface.h \
${APP_DIR}/pjs_lua_libs.h ${APP_DIR}/pjs_vr.h ${APP_DIR}/libconfig/libconfig.hpp

LINUX_INCLUDE=-I ${APP_DIR}
LINUX_LIBDIR=
CFLAGS=
SWIG=swig

all : nativesrc/intercom_lua_wrap.cpp 

nativesrc/intercom_lua_wrap.cpp : intercom_lua.i
	mkdir -p nativesrc
	${SWIG} ${CFLAGS} -Wall -o nativesrc/intercom_lua_wrap.cpp -lua -c++ intercom_lua.i
	${SWIG} ${CFLAGS} -Wall -lua -c++ -external-runtime nativesrc/swig_lua_runtime.h
	#rm intercom_lua.i

intercom_lua.i : ${APP_HEADERS} intercom_lua.i_header
	echo > intercom_lua.i_body
	for f in ${APP_HEADERS}; do ./clean_header_for_swig.py $$f >> intercom_lua.i_body; done
	cat intercom_lua.i_header intercom_lua.i_body intercom_lua.i_footer > intercom_lua.i
	rm intercom_lua.i_body || true
	
clean :
	rm intercom_lua.i nativesrc/intercom_lua_wrap.cpp intercom_lua_wrap.o || true

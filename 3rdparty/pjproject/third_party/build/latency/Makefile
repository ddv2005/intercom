include ../../../build.mak
include ../../../build/common.mak

PJLIB_LIB:=../../pjlib/lib/libpj-$(TARGET_NAME)$(LIBEXT)
PJLIB_UTIL_LIB:=../../pjlib-util/lib/libpjlib-util-$(TARGET_NAME)$(LIBEXT)
PJNATH_LIB:=../../pjnath/lib/libpjnath-$(TARGET_NAME)$(LIBEXT)
PJMEDIA_LIB:=../../pjmedia/lib/libpjmedia-$(TARGET_NAME)$(LIBEXT)
PJMEDIA_AUDIODEV_LIB:=../../pjmedia/lib/libpjmedia-audiodev-$(TARGET_NAME)$(LIBEXT)
PJMEDIA_CODEC_LIB:=../../pjmedia/lib/libpjmedia-codec-$(TARGET_NAME)$(LIBEXT)

export LIBDIR := ../../lib

RULES_MAK := $(PJDIR)/build/rules.mak

export LATENCY_EXE := ../../bin/latency-$(TARGET_NAME)$(HOST_EXE)

###############################################################################
# Gather all flags.
#
export LATENCY_SRCDIR = ../../latency
export LATENCY_OBJS = main.o tones.o latency.o pjsupport/src/pj_support.o pjsupport/src/pj_logger.o

export LATENCY_CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC). $(CC_INC)../../latency \
		   $(CC_INC)../../../pjlib/include
export LATENCY_CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS) \
		   $(CC_INC)../../../pjlib/include		   \
		   $(CC_INC)../../../pjlib-util/include		   \
		   $(CC_INC)$(LATENCY_SRCDIR)/pjsupport/include		   \
		   $(CC_INC)../../../pjmedia/include		   		   
export LATENCY_LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS)  $(APP_LDLIBS) $(APP_LDFLAGS) -lstdc++


export LATENCY_CFLAGS = -DSASR -DWAV49 -DNeedFunctionPrototypes=1 $(_CFLAGS)


export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := latency

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: dep depend latency clean realclean distclean

latency:
	$(MAKE) -f $(RULES_MAK) APP=LATENCY app=latency_ $(LATENCY_EXE)

clean print_lib:
	$(MAKE) -f $(RULES_MAK) APP=LATENCY app=latency_ $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.ilbc-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=LATENCY app=latemcy_ $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=LATENCY app=latency_ $@



# @configure_input@
include ../../../build.mak
include $(PJDIR)/build/common.mak

export LIBDIR := ../../lib

RULES_MAK := $(PJDIR)/build/rules.mak

export PORTAUDIO_LIB:=../../lib/libportaudio-$(TARGET_NAME)$(LIBEXT)

PORTAUDIO_OBJS += pa_allocation.o \
		  pa_converters.o \
		  pa_cpuload.o \
		  pa_dither.o \
		  pa_debugprint.o \
		  pa_front.o \
		  pa_process.o \
		  pa_skeleton.o \
		  pa_stream.o \
		  pa_trace.o


###############################################################################
# Gather all flags.
#
export _CFLAGS 	+= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC). $(CC_INC)src 
export _CXXFLAGS+= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS += $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

###############################################################################
# Defines for building PORTAUDIO library
#
export PORTAUDIO_SRCDIR = src
export PORTAUDIO_OBJS += 
export PORTAUDIO_CFLAGS += $(_CFLAGS)

export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := libportaudio

all: $(TARGETS)

dep: depend
distclean: realclean

.PHONY: dep depend libportaudio clean realclean distclean

libportaudio:
	$(MAKE) -f $(RULES_MAK) APP=PORTAUDIO app=libportaudio $(PORTAUDIO_LIB)

clean:
	$(MAKE) -f $(RULES_MAK) APP=PORTAUDIO app=libportaudio $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.libportaudio-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=PORTAUDIO app=libportaudio $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=PORTAUDIO app=libportaudio $@


include ../../../build.mak
include ../../../build/common.mak

export LIBDIR := ../../lib

RULES_MAK := $(PJDIR)/build/rules.mak

export RESAMPLE_LIB := ../../lib/libresample-$(TARGET_NAME)$(LIBEXT)

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC). $(CC_INC)../../resample/include \
		   $(CC_INC)../../../pjlib/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

export RESAMPLE_SRCDIR = ../../resample/src
export RESAMPLE_OBJS = resamplesubs.o
export RESAMPLE_CFLAGS = $(_CFLAGS)


export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := libresample

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: dep depend libresample clean realclean distclean

libresample:
	$(MAKE) -f $(RULES_MAK) APP=RESAMPLE app=libresample $(RESAMPLE_LIB)

clean print_lib:
	$(MAKE) -f $(RULES_MAK) APP=RESAMPLE app=libresample $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.ilbc-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=RESAMPLE app=libresample $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=RESAMPLE app=libresample $@



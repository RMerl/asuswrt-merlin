include ../../build.mak
include $(PJDIR)/build/common.mak


RULES_MAK := $(PJDIR)/build/rules.mak

PJLIB_LIB:=../../pjlib/lib/libpj-$(TARGET_NAME)$(LIBEXT)
PJLIB_UTIL_LIB:=../../pjlib-util/lib/libpjlib-util-$(TARGET_NAME)$(LIBEXT)
PJNATH_LIB:=../../pjnath/lib/libpjnath-$(TARGET_NAME)$(LIBEXT)
PJMEDIA_LIB:=../../pjmedia/lib/libpjmedia-$(TARGET_NAME)$(LIBEXT)
PJMEDIA_AUDIODEV_LIB:=../../pjmedia/lib/libpjmedia-audiodev-$(TARGET_NAME)$(LIBEXT)
PJMEDIA_CODEC_LIB:=../../pjmedia/lib/libpjmedia-codec-$(TARGET_NAME)$(LIBEXT)
PJSIP_LIB:=../../pjsip/lib/libpjsip-$(TARGET_NAME)$(LIBEXT)
PJSIP_UA_LIB:=../../pjsip/lib/libpjsip-ua-$(TARGET_NAME)$(LIBEXT)
PJSIP_SIMPLE_LIB:=../../pjsip/lib/libpjsip-simple-$(TARGET_NAME)$(LIBEXT)
PJSUA_LIB_LIB=../../pjsip/lib/libpjsua-$(TARGET_NAME)$(LIBEXT)


###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../../pjsip/include \
		   $(CC_INC)../../pjlib/include \
		   $(CC_INC)../../pjlib-util/include \
		   $(CC_INC)../../pjnath/include \
		   $(CC_INC)../../pjmedia/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)

###############################################################################
# Defines for building PJSUA
#
export PJSUA_SRCDIR = ../src/pjsua
export PJSUA_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			main.o pjsua_app.o
export PJSUA_CFLAGS += $(_CFLAGS)
export PJSUA_LDFLAGS += $(APP_LDFLAGS) $(APP_LDLIBS) $(LDFLAGS)
export PJSUA_EXE:=../bin/pjsua-$(TARGET_NAME)$(HOST_EXE)


###############################################################################
# Defines for building pjsystest
#
export PJSYSTEST_SRCDIR = ../src/pjsystest
export PJSYSTEST_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			systest.o main_console.o
export PJSYSTEST_CFLAGS += $(_CFLAGS)
export PJSYSTEST_LDFLAGS += $(APP_LDFLAGS) $(APP_LDLIBS) $(LDFLAGS)
export PJSYSTEST_EXE:=../bin/pjsystest-$(TARGET_NAME)$(HOST_EXE)


export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
#
TARGETS := pjsua pjsystest samples

.PHONY: $(TARGETS)

all: $(TARGETS)

doc:

dep: depend
distclean: realclean

.PHONY: dep depend pjsua clean realclean distclean

pjsua:
	$(MAKE) -f $(RULES_MAK) APP=PJSUA app=pjsua $(PJSUA_EXE)

pjsystest:
	$(MAKE) -f $(RULES_MAK) APP=PJSYSTEST app=pjsystest $(PJSYSTEST_EXE)

samples:
	$(MAKE) -f Samples.mak

.PHONY: ../lib/pjsua.ko
../lib/pjsua.ko:
	$(MAKE) -f $(RULES_MAK) APP=PJSUA app=pjsua $@

clean depend realclean:
	$(MAKE) -f $(RULES_MAK) APP=PJSUA app=pjsua $@
	$(MAKE) -f $(RULES_MAK) APP=PJSYSTEST app=pjsystest $@
	$(MAKE) -f Samples.mak $@
	@if test "$@" = "depend"; then \
	  echo '$(PJSUA_EXE): $(APP_LIB_FILES)' >> .pjsua-$(TARGET_NAME).depend; \
	  echo '$(PJSYSTEST_EXE): $(APP_LIB_FILES)' >> .pjsystest-$(TARGET_NAME).depend; \
	fi




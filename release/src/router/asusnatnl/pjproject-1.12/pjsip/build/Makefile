# For common OSes, test's main() is defined in main.c.
# OS specific configuration may want to put it in different file.
# For example, see os-rtems.mak in current directory.
export TEST_OBJS = main.o

include ../../build.mak
include ../../version.mak
include $(PJDIR)/build/common.mak

THIRD_PARTY:=$(PJDIR)/third_party

UMEM_INC=$(CC_INC)$(THIRD_PARTY)/umem
UMEM_LIBS=-L$(THIRD_PARTY)/lib -lumem-$(TARGET_NAME) -ldl

RULES_MAK := $(PJDIR)/build/rules.mak

PJLIB_LIB:=../../pjlib/lib/libpj-$(TARGET_NAME)$(LIBEXT)
PJLIB_UTIL_LIB:=../../pjlib-util/lib/libpjlib-util-$(TARGET_NAME)$(LIBEXT)
PJMEDIA_LIB:=../../pjmedia/lib/libpjmedia-$(TARGET_NAME)$(LIBEXT)
PJMEDIA_CODEC_LIB:=../../pjmedia/lib/libpjmedia-codec-$(TARGET_NAME)$(LIBEXT)

export PJSIP_LIB:=../lib/libpjsip-$(TARGET_NAME)$(LIBEXT)
export PJSIP_UA_LIB:=../lib/libpjsip-ua-$(TARGET_NAME)$(LIBEXT)
export PJSIP_SIMPLE_LIB:=../lib/libpjsip-simple-$(TARGET_NAME)$(LIBEXT)
export PJSUA_LIB_LIB=../lib/libpjsua-$(TARGET_NAME)$(LIBEXT)


###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../include \
		   $(CC_INC)../../pjlib/include \
		   $(CC_INC)../../pjlib-util/include \
		   $(CC_INC)../../pjnath/include \
		   $(CC_INC)../../pjmedia/include \
		   $(UMEM_INC)
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)

###############################################################################
# Defines for building PJSIP core library
#
export PJSIP_SRCDIR = ../src/pjsip
export PJSIP_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
		sip_config.o sip_multipart.o \
		sip_errno.o sip_msg.o sip_parser.o sip_tel_uri.o sip_uri.o \
		sip_endpoint.o sip_util.o sip_util_proxy.o \
		sip_resolve.o sip_transport.o sip_transport_loop.o \
		sip_transport_udp.o sip_transport_tcp.o \
		sip_transport_tls.o sip_auth_aka.o sip_auth_client.o \
		sip_auth_msg.o sip_auth_parser.o \
		sip_auth_server.o \
		sip_transaction.o sip_util_statefull.o \
		sip_dialog.o sip_ua_layer.o
export PJSIP_CFLAGS += $(_CFLAGS)

###############################################################################
# Defines for building PJSIP UA library
#
export PJSIP_UA_SRCDIR = ../src/pjsip-ua
export PJSIP_UA_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			sip_inv.o sip_reg.o sip_replaces.o sip_xfer.o \
			sip_100rel.o sip_timer.o
export PJSIP_UA_CFLAGS += $(_CFLAGS)


###############################################################################
# Defines for building PJSIP SIMPLE library
#
export PJSIP_SIMPLE_SRCDIR = ../src/pjsip-simple
export PJSIP_SIMPLE_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			evsub.o evsub_msg.o iscomposing.o \
			mwi.o pidf.o presence.o presence_body.o publishc.o \
			rpid.o xpidf.o \
			errno_sipsimple.o
#charles move errno.o -> errno_sipsimple.o
export PJSIP_SIMPLE_CFLAGS += $(_CFLAGS)


###############################################################################
# Defines for building PJSUA-LIB library
#
export PJSUA_LIB_SRCDIR = ../src/pjsua-lib
export PJSUA_LIB_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			pjsua_acc.o pjsua_call.o pjsua_core.o \
			pjsua_im.o pjsua_media.o pjsua_pres.o
export PJSUA_LIB_CFLAGS += $(_CFLAGS)


export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 

###############################################################################
# Defines for building test application
#
export TEST_SRCDIR = ../src/test
export TEST_OBJS += dlg_core_test.o dns_test.o msg_err_test.o \
		    msg_logger.o msg_test.o multipart_test.o regc_test.o \
		    test.o transport_loop_test.o transport_tcp_test.o \
		    transport_test.o transport_udp_test.o \
		    tsx_basic_test.o tsx_bench.o tsx_uac_test.o \
		    tsx_uas_test.o txdata_test.o uri_test.o \
		    inv_offer_answer_test.o
export TEST_CFLAGS += $(_CFLAGS)
export TEST_LDFLAGS += $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS) \
		       $(UMEM_LIBS)
export TEST_EXE := ../bin/pjsip-test-$(TARGET_NAME)$(HOST_EXE)

	
export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
#
TARGETS := pjsip pjsip-ua pjsip-simple pjsua-lib pjsip-test

.PHONY: $(TARGETS)

all: $(TARGETS)

doc:
	cd .. && rm -rf docs/$(PJ_VERSION) && doxygen docs/doxygen.cfg
	@if [ -n "$(WWWDIR)" ] && ! [ -d "$(WWWDIR)/docs/$(PJ_VERSION)/pjsip/docs/html" ] ; then \
		echo "Creating docs/$(PJ_VERSION)/pjsip/docs/html" ; \
		mkdir -p $(WWWDIR)/docs/$(PJ_VERSION)/pjsip/docs/html ; \
	fi 
	@if [ -n "$(WWWDIR)" ] && [ -d "$(WWWDIR)/docs/$(PJ_VERSION)/pjsip/docs/html" ] ; then \
		echo "Copying docs/$(PJ_VERSION) to $(WWWDIR)/docs/$(PJ_VERSION)/pjsip/docs/html.." ; \
		cp -v -a ../docs/$(PJ_VERSION)/html/* $(WWWDIR)/docs/$(PJ_VERSION)/pjsip/docs/html/ ; \
	fi

dep: depend
distclean: realclean

.PHONY: dep depend pjsip pjsip-ua pjsua-lib clean realclean distclean

pjsip:
	$(MAKE) -f $(RULES_MAK) APP=PJSIP app=pjsip $(PJSIP_LIB)

pjsip-ua:
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_UA app=pjsip-ua $(PJSIP_UA_LIB)

pjsip-simple:
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_SIMPLE app=pjsip-simple $(PJSIP_SIMPLE_LIB)

pjsua-lib: 
	$(MAKE) -f $(RULES_MAK) APP=PJSUA_LIB app=pjsua-lib $(PJSUA_LIB_LIB)

pjsip-test:
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjsip-test $(TEST_EXE)

.PHONY: ../lib/pjsip.ko
../lib/pjsip.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP app=pjsip $@

.PHONY: ../lib/pjsip-ua.ko
../lib/pjsip-ua.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_UA app=pjsip-ua $@

.PHONY: ../lib/pjsip-simple.ko
../lib/pjsip-simple.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_SIMPLE app=pjsip-simple $@

.PHONY: ../lib/pjsua-lib.ko
../lib/pjsua-lib.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJSUA_LIB app=pjsua-lib $@

clean:
	$(MAKE) -f $(RULES_MAK) APP=PJSIP app=pjsip $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_UA app=pjsip-ua $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_SIMPLE app=pjsip-simple $@
	$(MAKE) -f $(RULES_MAK) APP=PJSUA_LIB app=pjsua-lib $@
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjsip-test $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=PJSIP app=pjsip $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_UA app=pjsip-ua $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_SIMPLE app=pjsip-simple $@
	$(MAKE) -f $(RULES_MAK) APP=PJSUA_LIB app=pjsua-lib $@
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjsip-test $@
	echo '$(TEST_EXE): $(PJMEDIA_LIB) $(PJSUA_LIB_LIB) $(PJSIP_SIMPLE_LIB) $(PJSIP_UA_LIB) $(PJSIP_LIB) $(PJNATH_LIB) $(PJLIB_UTIL_LIB) $(PJLIB_LIB)' >> .pjsip-test-$(TARGET_NAME).depend

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.pjsip-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjsip-ua-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjsip-simple-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjsua-lib-$(TARGET_NAME).depend),$(HOST_RMR))
	$(MAKE) -f $(RULES_MAK) APP=PJSIP app=pjsip $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_UA app=pjsip-ua $@
	$(MAKE) -f $(RULES_MAK) APP=PJSIP_SIMPLE app=pjsip-simple $@
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjsip-test $@
	$(MAKE) -f $(RULES_MAK) APP=PJSUA_LIB app=pjsua-lib $@




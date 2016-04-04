include ../../build.mak
include ../../version.mak

THIRD_PARTY:=$(PJDIR)/third_party

SRTP_INC=$(CC_INC)$(THIRD_PARTY)/build/srtp \
	 $(CC_INC)$(THIRD_PARTY)/srtp/crypto/include \
	 $(CC_INC)$(THIRD_PARTY)/srtp/include

BZIP2_INC=$(CC_INC)$(THIRD_PARTY)/bzip2
BZIP2_LIBS=-lbzip2-$(TARGET_NAME)

USRSCTP_INC=$(CC_INC)$(THIRD_PARTY)/usrsctp/usrsctplib
USRSCTP_LIBS=-lusrsctp-$(TARGET_NAME)

include $(PJDIR)/build/common.mak

RULES_MAK := $(PJDIR)/build/rules.mak

PJLIB_LIB:=$(PJDIR)/pjlib/lib/libpj-$(TARGET_NAME)$(LIBEXT)
PJLIB_UTIL_LIB:=$(PJDIR)/pjlib-util/lib/libpjlib-util-$(TARGET_NAME)$(LIBEXT)
PJNATH_LIB:=$(PJDIR)/pjnath/lib/libpjnath-$(TARGET_NAME)$(LIBEXT)

export PJMEDIA_LIB:=../lib/libpjmedia-$(TARGET_NAME)$(LIBEXT)
export PJMEDIA_CODEC_LIB:=../lib/libpjmedia-codec-$(TARGET_NAME)$(LIBEXT)
export PJSDP_LIB:=../lib/libpjsdp-$(TARGET_NAME)$(LIBEXT)
export PJMEDIA_AUDIODEV_LIB:=../lib/libpjmedia-audiodev-$(TARGET_NAME)$(LIBEXT)


###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../include \
		   $(CC_INC)../../pjlib/include \
		   $(CC_INC)../../pjlib-util/include \
		   $(CC_INC)../../pjnath/include \
		   $(CC_INC)../../pjsip/include \
		   $(CC_INC)../.. \
		   $(SRTP_INC) \
		   $(BZIP2_INC) \
		   $(USRSCTP_INC)

export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(subst /,$(HOST_PSEP),$(PJMEDIA_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJMEDIA_AUDIODEV_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJMEDIA_CODEC_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJLIB_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJLIB_UTIL_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJNATH_LIB)) \
		   -L$(PJDIR)/third_party/lib \
		   $(APP_THIRD_PARTY_LIBS) \
		   $(APP_THIRD_PARTY_EXT) \
		   $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

###############################################################################
# Defines for building PJMEDIA library
#
export PJMEDIA_SRCDIR = ../src/pjmedia
export PJMEDIA_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			alaw_ulaw.o alaw_ulaw_table.o bidirectional.o clock_thread.o codec.o \
			conference.o conf_switch.o delaybuf.o echo_common.o \
			echo_port.o echo_suppress.o endpoint.o errno.o \
			g711.o jbuf.o master_port.o mem_capture.o mem_player.o \
			null_port.o plc_common.o port.o splitcomb.o \
			resample_resample.o resample_libsamplerate.o \
			resample_port.o rtcp.o rtcp_xr.o rtp.o \
			sdp.o sdp_cmp.o sdp_neg.o session.o silencedet.o \
			sound_legacy.o sound_port.o stereo_port.o \
			stream.o natnl_stream.o tonegen.o transport_adapter_sample.o \
			transport_ice.o transport_loop.o transport_sctp.o \
			transport_srtp.o transport_dtls.o transport_udp.o transport_tcp.o\
			wav_player.o wav_playlist.o wav_writer.o wave.o \
			wsola.o

export PJMEDIA_CFLAGS += $(_CFLAGS)


###############################################################################
# Defines for building PJMEDIA-AUDIODEV library
#
export PJMEDIA_AUDIODEV_SRCDIR = ../src/pjmedia-audiodev
export PJMEDIA_AUDIODEV_OBJS +=  audiodev.o audiotest.o errno.o \
				 coreaudio_dev.o legacy_dev.o null_dev.o pa_dev.o wmme_dev.o \
				 alsa_dev.o
export PJMEDIA_AUDIODEV_CFLAGS += $(_CFLAGS)


###############################################################################
# Defines for building PJSDP library
# Note that SDP functionality is already INCLUDED in PJMEDIA.
# The PJSDP library should only be used for applications that want SDP
# but don't want to use the rest of the media framework.
#
export PJSDP_SRCDIR = ../src/pjmedia
export PJSDP_OBJS += 	$(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			errno.o sdp.o sdp_cmp.o sdp_neg.o

ifneq (,$(findstring darwin,$(TARGET_NAME)))
    USER_SPACE_CFLAGS+=-D__APPLE_USE_RFC_2292 -U__APPLE__ -D__Userspace_os_Darwin -DHAVE_SCONN_LEN
endif

export PJSDP_CFLAGS += $(_CFLAGS) $(USER_SPACE_CFLAGS)


###############################################################################
# Defines for building PJMEDIA-Codec library
#
export PJMEDIA_CODEC_SRCDIR = ../src/pjmedia-codec
export PJMEDIA_CODEC_OBJS += \
			$(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
			ipp_codecs.o $(CODEC_OBJS)
export PJMEDIA_CODEC_CFLAGS += $(_CFLAGS) $(GSM_CFLAGS) $(SPEEX_CFLAGS) \
			$(ILBC_CFLAGS) $(IPP_CFLAGS) $(G7221_CFLAGS)


###############################################################################
# Defines for building test application
#
export PJMEDIA_TEST_SRCDIR = ../src/test
export PJMEDIA_TEST_OBJS += codec_vectors.o jbuf_test.o main.o mips_test.o rtp_test.o test.o
export PJMEDIA_TEST_OBJS += sdp_neg_test.o 
export PJMEDIA_TEST_CFLAGS += $(_CFLAGS)
export PJMEDIA_TEST_LDFLAGS += $(_LDFLAGS)
export PJMEDIA_TEST_EXE:=../bin/pjmedia-test-$(TARGET_NAME)$(HOST_EXE)

	
export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := pjmedia pjmedia-audiodev pjmedia-codec pjsdp pjmedia-test

all: $(TARGETS)

doc:
	cd .. && rm -rf docs/$(PJ_VERSION) && doxygen docs/doxygen.cfg
	@if [ -n "$(WWWDIR)" ] && ! [ -d "$(WWWDIR)/docs/$(PJ_VERSION)/pjmedia/docs/html" ] ; then \
		echo "Creating docs/$(PJ_VERSION)/pjmedia/docs/html" ; \
		mkdir -p $(WWWDIR)/docs/$(PJ_VERSION)/pjmedia/docs/html ; \
	fi 
	@if [ -n "$(WWWDIR)" ] && [ -d "$(WWWDIR)/docs/$(PJ_VERSION)/pjmedia/docs/html" ] ; then \
		echo "Copying docs/$(PJ_VERSION) to $(WWWDIR)/docs/$(PJ_VERSION)/pjmedia/docs/html.." ; \
		cp -v -a ../docs/$(PJ_VERSION)/html/* $(WWWDIR)/docs/$(PJ_VERSION)/pjmedia/docs/html/ ; \
	fi

dep: depend
distclean: realclean

.PHONY: dep depend pjmedia pjmedia-codec pjmedia-audiodev pjmedia-test clean realclean distclean

pjmedia:
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA app=pjmedia $(PJMEDIA_LIB)

pjmedia-codec:
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_CODEC app=pjmedia-codec $(PJMEDIA_CODEC_LIB)

pjmedia-audiodev:
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_AUDIODEV app=pjmedia-audiodev $(PJMEDIA_AUDIODEV_LIB)

pjsdp:
	$(MAKE) -f $(RULES_MAK) APP=PJSDP app=pjsdp $(PJSDP_LIB)

pjmedia-test: $(PJMEDIA_LIB)
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_TEST app=pjmedia-test $(PJMEDIA_TEST_EXE)

.PHONY: ../lib/pjmedia.ko
../lib/pjmedia.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA app=pjmedia $@

.PHONY: ../lib/pjmedia-codec.ko
../lib/pjmedia-codec.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_CODEC app=pjmedia-codec $@

.PHONY: ../lib/pjmedia-test.ko
../lib/pjmedia-test.ko:
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_TEST app=pjmedia-test $@

clean:
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA app=pjmedia $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_CODEC app=pjmedia-codec $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_AUDIODEV app=pjmedia-audiodev $@
	$(MAKE) -f $(RULES_MAK) APP=PJSDP app=pjsdp $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_TEST app=pjmedia-test $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.pjmedia-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjmedia-audiodev-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjmedia-codec-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjmedia-test-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjsdp-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA app=pjmedia $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_AUDIODEV app=pjmedia-audiodev $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_CODEC app=pjmedia-codec $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_TEST app=pjmedia-test $@
	$(MAKE) -f $(RULES_MAK) APP=PJSDP app=pjsdp $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA app=pjmedia $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_AUDIODEV app=pjmedia-audiodev $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_CODEC app=pjmedia-codec $@
	$(MAKE) -f $(RULES_MAK) APP=PJMEDIA_TEST app=pjmedia-test $@
	$(MAKE) -f $(RULES_MAK) APP=PJSDP app=pjsdp $@
	echo '$(PJMEDIA_TEST_EXE): $(PJMEDIA_LIB) $(PJMEDIA_CODEC_LIB) $(PJNATH_LIB) $(PJLIB_UTIL_LIB) $(PJLIB_LIB)' >> .pjmedia-test-$(TARGET_NAME).depend



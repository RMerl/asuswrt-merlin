#
# This is a make file for performing various tests on the libraries
#
# Sample user.mak contents:
#  export CFLAGS += -Wno-unused-label -Werror
#
#  ifeq ($(CPP_MODE),1)
#  export CFLAGS += -x c++
#  export LDFLAGS += -lstdc++
#  endif

PJSUA_OPT=--null-audio


build_test: distclean rm_build_mak build_mak everything cpp_prep cpp_test cpp_post everything
 
run_test: pjlib_test pjlib_util_test pjnath_test pjsip_test pjsua_test

all: build_test run_test

CPP_DIR=pjlib pjlib-util pjnath pjmedia pjsip

.PHONY: build_test distclean rm_build_mak build_mak everything pjlib_test pjlib_util_test pjnath_test pjsip_test cpp_prep cpp_test cpp_post pjsua_test

distclean:
	make distclean

rm_build_mak:
	rm -f build.mak

build_mak:
	./configure
	make dep

everything: 
	make

pjlib_test:
	cd pjlib/bin && ./pjlib-test-`../../config.guess`

pjlib_util_test:
	cd pjlib-util/bin && ./pjlib-util-test-`../../config.guess`

pjnath_test:
	cd pjnath/bin && ./pjnath-test-`../../config.guess`

pjsip_test:
	cd pjsip/bin && ./pjsip-test-`../../config.guess`

cpp_prep:
	for dir in $(CPP_DIR); do \
		make -C $$dir/build clean; \
	done

cpp_test:
	make -f c++-build.mak

cpp_post:
	make -f c++-build.mak clean

pjsua_test: pjsua_config_file pjsua_local_port0 pjsua_ip_addr pjsua_no_tcp pjsua_no_udp pjsua_outbound pjsua_use_ice pjsua_add_codec pjsua_clock_rate pjsua_play_file pjsua_play_tone pjsua_rec_file pjsua_rtp_port pjsua_quality pjsua_ptime pjsua_ectail
	@echo pjsua_test completed successfully

pjsua_config_file:
	touch testconfig.cfg
	echo q | pjsip-apps/bin/pjsua-`./config.guess` $(PJSUA_OPT) --config-file testconfig.cfg
	rm -f testconfig.cfg

pjsua_local_port0:
	echo q | pjsip-apps/bin/pjsua-`./config.guess` $(PJSUA_OPT) --local-port 0

pjsua_ip_addr:
	echo q | pjsip-apps/bin/pjsua-`./config.guess` $(PJSUA_OPT) --ip-addr 1.1.1.1

pjsua_no_tcp:
	echo q | pjsip-apps/bin/pjsua-`./config.guess` $(PJSUA_OPT) --no-tcp

pjsua_no_udp:
	echo q | pjsip-apps/bin/pjsua-`./config.guess` $(PJSUA_OPT) --no-udp

pjsua_outbound:
	echo q | pjsip-apps/bin/pjsua-`./config.guess` $(PJSUA_OPT) --outbound 'sip:1.2.3.4;lr'

pjsua_use_ice:
	echo q | pjsip-apps/bin/pjsua-`./config.guess` $(PJSUA_OPT) --use-ice

pjsua_add_codec:
	echo q | pjsip-apps/bin/pjsua-`./config.guess` $(PJSUA_OPT) --add-codec pcma

pjsua_clock_rate:
	echo q | pjsip-apps/bin/pjsua-`./config.guess` $(PJSUA_OPT) --clock-rate 8000

pjsua_play_file:
	echo q | pjsip-apps/bin/pjsua-`./config.guess` $(PJSUA_OPT) --play-file pjsip-apps/bin/d16.wav --auto-play --auto-loop --auto-conf

pjsua_play_tone:
	echo q | pjsip-apps/bin/pjsua-`./config.guess` $(PJSUA_OPT) --play-tone '400,600,100,500'

pjsua_rec_file:
	echo q | pjsip-apps/bin/pjsua-`./config.guess` $(PJSUA_OPT) --rec-file pjsip-apps/bin/testrec.wav --auto-rec

pjsua_rtp_port:
	echo q | pjsip-apps/bin/pjsua-`./config.guess` $(PJSUA_OPT) --rtp-port 8000

pjsua_quality:
	echo q | pjsip-apps/bin/pjsua-`./config.guess` $(PJSUA_OPT) --quality 10

pjsua_ptime:
	echo q | pjsip-apps/bin/pjsua-`./config.guess` $(PJSUA_OPT) --ptime 40

pjsua_ectail:
	echo q | pjsip-apps/bin/pjsua-`./config.guess` $(PJSUA_OPT) --ec-tail 10


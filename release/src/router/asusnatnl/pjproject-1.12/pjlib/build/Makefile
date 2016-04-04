include ../../build.mak
include ../../version.mak
include $(PJDIR)/build/common.mak

THIRD_PARTY:=$(PJDIR)/third_party

UMEM_INC=$(CC_INC)$(THIRD_PARTY)/umem
UMEM_LIBS=-L$(THIRD_PARTY)/lib -lumem-$(TARGET_NAME)
UMEM_CFLAGS=-DUMEM_STANDALONE

RULES_MAK := $(PJDIR)/build/rules.mak


export PJLIB_LIB := ../lib/libpj-$(TARGET_NAME)$(LIBEXT)

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) \
       $(UMEM_CFLAGS) \
       $(CC_INC)../include \
		   $(UMEM_INC)
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(subst /,$(HOST_PSEP),$(PJLIB_LIB)) \
		   $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) \
		   $(UMEM_LIBS)

###############################################################################
# Defines for building PJLIB library
#
export PJLIB_SRCDIR = ../src/pj
export PJLIB_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
	activesock.o array.o config.o ctype.o except.o fifobuf.o \
	guid.o hash.o ip_helper_generic.o list.o lock.o log.o os_time_common.o \
	os_info.o pool.o pool_buf.o pool_caching.o pool_dbg.o rand.o \
	rbtree.o sock_common.o sock_qos_common.o sock_qos_bsd.o \
	ssl_sock_common.o ssl_sock_ossl.o ssl_sock_dump.o \
	timer.o types.o \
	errno_lib.o string_lib.o bandwidth.o
#charles move errno.o to errno_lib.o, string.o to string_lib.o
export PJLIB_CFLAGS += $(_CFLAGS)

###############################################################################
# Defines for building test application
#
export TEST_SRCDIR = ../src/pjlib-test
export TEST_OBJS += activesock.o atomic.o echo_clt.o errno.o exception.o \
		    fifobuf.o file.o hash_test.o ioq_perf.o ioq_udp.o \
		    ioq_unreg.o ioq_tcp.o \
		    list.o mutex.o os.o pool.o pool_perf.o rand.o rbtree.o \
		    select.o sleep.o sock.o sock_perf.o ssl_sock.o \
		    string.o test.o thread.o timer.o timestamp.o \
		    udp_echo_srv_sync.o udp_echo_srv_ioqueue.o \
		    util.o
export TEST_CFLAGS += $(_CFLAGS)
export TEST_LDFLAGS += $(_LDFLAGS)
export TEST_EXE := ../bin/pjlib-test-$(TARGET_NAME)$(HOST_EXE)

	
export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#

all: $(TARGETS)

doc:
	cd .. && rm -rf docs/$(PJ_VERSION) && doxygen docs/doxygen.cfg
	@if [ -n "$(WWWDIR)" ] && ! [ -d "$(WWWDIR)/docs/$(PJ_VERSION)/pjlib/docs/html" ] ; then \
		echo "Creating docs/$(PJ_VERSION)/pjlib/docs/html" ; \
		mkdir -p $(WWWDIR)/docs/$(PJ_VERSION)/pjlib/docs/html ; \
	fi 
	@if [ -n "$(WWWDIR)" ] && [ -d "$(WWWDIR)/docs/$(PJ_VERSION)/pjlib/docs/html" ] ; then \
		echo "Copying docs/$(PJ_VERSION) to $(WWWDIR)/docs/$(PJ_VERSION)/pjlib/docs/html.." ; \
		cp -v -a ../docs/$(PJ_VERSION)/html/* $(WWWDIR)/docs/$(PJ_VERSION)/pjlib/docs/html/ ; \
	fi
print:
	$(MAKE) -f $(RULES_MAK) APP=PJLIB app=pjlib print_lib
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjlib-test print_bin
	
depend: ../include/pj/config_site.h
	$(MAKE) -f $(RULES_MAK) APP=PJLIB app=pjlib depend
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjlib-test depend
	echo '$(TEST_EXE): $(PJLIB_LIB)' >> .pjlib-test-$(TARGET_NAME).depend


.PHONY: dep depend pjlib pjlib-test clean realclean distclean

dep: depend

pjlib: ../include/pj/config_site.h
	$(MAKE) -f $(RULES_MAK) APP=PJLIB app=pjlib $(PJLIB_LIB)

../include/pj/config_site.h:
	touch ../include/pj/config_site.h
	
pjlib-test: 
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjlib-test $(TEST_EXE)

.PHONY: ../lib/pjlib.ko
../lib/pjlib.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJLIB app=pjlib $@

.PHONY: ../lib/pjlib-test.ko
../lib/pjlib-test.ko:
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjlib-test $@

clean:
	$(MAKE) -f $(RULES_MAK) APP=PJLIB app=pjlib clean
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjlib-test clean

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.pjlib-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjlib-test-$(TARGET_NAME).depend),$(HOST_RMR))
	$(MAKE) -f $(RULES_MAK) APP=PJLIB app=pjlib realclean
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjlib-test realclean

distclean: realclean

gcov-report:
	$(MAKE) -f $(RULES_MAK) APP=PJLIB app=pjlib gcov-report
	$(MAKE) -f $(RULES_MAK) APP=TEST app=pjlib-test gcov-report


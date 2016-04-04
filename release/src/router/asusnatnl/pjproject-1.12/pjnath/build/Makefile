# By default, the test application includes main.o.
# OS make file may override this with os-specific files
export PJNATH_TEST_OBJS = main.o

include ../../build.mak
include ../../version.mak
include $(PJDIR)/build/common.mak

THIRD_PARTY:=$(PJDIR)/third_party

UMEM_INC=$(CC_INC)$(THIRD_PARTY)/umem
UMEM_LIBS=-L$(THIRD_PARTY)/lib -lumem-$(TARGET_NAME) -ldl

RULES_MAK := $(PJDIR)/build/rules.mak

PJLIB_LIB:=../../pjlib/lib/libpj-$(TARGET_NAME)$(LIBEXT)
PJLIB_UTIL_LIB:=../../pjlib-util/lib/libpjlib-util-$(TARGET_NAME)$(LIBEXT)
export PJNATH_LIB:=../lib/libpjnath-$(TARGET_NAME)$(LIBEXT)

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../include $(CC_INC)../../pjlib/include \
		   $(CC_INC)../../pjlib-util/include \
		   $(UMEM_INC)
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(subst /,$(HOST_PSEP),$(PJNATH_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJLIB_UTIL_LIB)) \
		   $(subst /,$(HOST_PSEP),$(PJLIB_LIB)) \
		   $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) \
		   $(UMEM_LIBS)

###############################################################################
# Defines for building PJNATH library
#
export PJNATH_SRCDIR = ../src/pjnath
export PJNATH_OBJS += $(OS_OBJS) $(M_OBJS) $(CC_OBJS) $(HOST_OBJS) \
		ice_session.o ice_strans.o nat_detect.o stun_auth.o \
		stun_msg.o stun_msg_dump.o stun_session.o stun_sock.o \
		stun_transaction.o turn_session.o turn_sock.o \
		tcp_session.o tcp_sock.o \
		errno_nath.o
#charles move errno.o to errno_nath.o
export PJNATH_CFLAGS += $(_CFLAGS)

###############################################################################
# Defines for building test application
#
export PJNATH_TEST_SRCDIR = ../src/pjnath-test
export PJNATH_TEST_OBJS += ice_test.o stun.o sess_auth.o server.o \
			    stun_sock_test.o turn_sock_test.o test.o
export PJNATH_TEST_CFLAGS += $(_CFLAGS)
export PJNATH_TEST_LDFLAGS += $(_LDFLAGS)
export PJNATH_TEST_EXE:=../bin/pjnath-test-$(TARGET_NAME)$(HOST_EXE)

	
###############################################################################
# Defines for building TURN client application
#
export PJTURN_CLIENT_SRCDIR = ../src/pjturn-client
export PJTURN_CLIENT_OBJS += client_main.o
export PJTURN_CLIENT_CFLAGS += $(_CFLAGS)
export PJTURN_CLIENT_LDFLAGS += $(_LDFLAGS)
export PJTURN_CLIENT_EXE:=../bin/pjturn-client-$(TARGET_NAME)$(HOST_EXE)

###############################################################################
# Defines for building TURN server application
#
export PJTURN_SRV_SRCDIR = ../src/pjturn-srv
export PJTURN_SRV_OBJS += allocation.o auth.o listener_udp.o \
			  listener_tcp.o server.o main.o
export PJTURN_SRV_CFLAGS += $(_CFLAGS)
export PJTURN_SRV_LDFLAGS += $(_LDFLAGS)
export PJTURN_SRV_EXE:=../bin/pjturn-srv-$(TARGET_NAME)$(HOST_EXE)

	
	
export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := pjnath pjnath-test pjturn-client pjturn-srv

all: $(TARGETS)

doc:
	cd .. && rm -rf docs/$(PJ_VERSION) && doxygen docs/doxygen.cfg
	@if [ -n "$(WWWDIR)" ] && ! [ -d "$(WWWDIR)/docs/$(PJ_VERSION)/pjnath/docs/html" ] ; then \
		echo "Creating docs/$(PJ_VERSION)/pjnath/docs/html" ; \
		mkdir -p $(WWWDIR)/docs/$(PJ_VERSION)/pjnath/docs/html ; \
	fi 
	@if [ -n "$(WWWDIR)" ] && [ -d "$(WWWDIR)/docs/$(PJ_VERSION)/pjnath/docs/html" ] ; then \
		echo "Copying docs/$(PJ_VERSION) to $(WWWDIR)/docs/$(PJ_VERSION)/pjnath/docs/html.." ; \
		cp -v -a ../docs/$(PJ_VERSION)/html/* $(WWWDIR)/docs/$(PJ_VERSION)/pjnath/docs/html/ ; \
	fi

dep: depend
distclean: realclean

.PHONY: dep depend pjlib pjlib-test clean realclean distclean

pjnath:
	$(MAKE) -f $(RULES_MAK) APP=PJNATH app=pjnath $(PJNATH_LIB)

pjnath-test: $(PJLIB_LIB) $(PJLIB_UTIL_LIB) $(PJNATH_LIB)
	$(MAKE) -f $(RULES_MAK) APP=PJNATH_TEST app=pjnath-test $(PJNATH_TEST_EXE)

pjturn-client: $(PJLIB_LIB) $(PJLIB_UTIL_LIB) $(PJNATH_LIB)
	$(MAKE) -f $(RULES_MAK) APP=PJTURN_CLIENT app=pjturn-client $(PJTURN_CLIENT_EXE)

pjturn-srv: $(PJLIB_LIB) $(PJLIB_UTIL_LIB) $(PJNATH_LIB)
	$(MAKE) -f $(RULES_MAK) APP=PJTURN_SRV app=pjturn-srv $(PJTURN_SRV_EXE)

.PHONY: ../lib/pjnath.ko
../lib/pjnath.ko:
	echo Making $@
	$(MAKE) -f $(RULES_MAK) APP=PJNATH app=pjnath $@

.PHONY: ../lib/pjnath-test.ko
../lib/pjnath-test.ko:
	$(MAKE) -f $(RULES_MAK) APP=PJNATH_TEST app=pjnath-test $@

clean:
	$(MAKE) -f $(RULES_MAK) APP=PJNATH app=pjnath $@
	$(MAKE) -f $(RULES_MAK) APP=PJNATH_TEST app=pjnath-test $@
	$(MAKE) -f $(RULES_MAK) APP=PJTURN_CLIENT app=pjturn-client $@
	$(MAKE) -f $(RULES_MAK) APP=PJTURN_SRV app=pjturn-srv $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.pjnath-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjnath-test-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjturn-client-$(TARGET_NAME).depend),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),.pjturn-srv-$(TARGET_NAME).depend),$(HOST_RMR))
	$(MAKE) -f $(RULES_MAK) APP=PJNATH app=pjnath $@
	$(MAKE) -f $(RULES_MAK) APP=PJNATH_TEST app=pjnath-test $@
	$(MAKE) -f $(RULES_MAK) APP=PJTURN_CLIENT app=pjturn-client $@
	$(MAKE) -f $(RULES_MAK) APP=PJTURN_SRV app=pjturn-srv $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=PJNATH app=pjnath $@
	$(MAKE) -f $(RULES_MAK) APP=PJNATH_TEST app=pjnath-test $@
	$(MAKE) -f $(RULES_MAK) APP=PJTURN_CLIENT app=pjturn-client $@
	$(MAKE) -f $(RULES_MAK) APP=PJTURN_SRV app=pjturn-srv $@
	echo '$(PJNATH_TEST_EXE): $(PJNATH_LIB) $(PJLIB_UTIL_LIB) $(PJLIB_LIB)' >> .pjnath-test-$(TARGET_NAME).depend
	echo '$(PJTURN_CLIENT_EXE): $(PJNATH_LIB) $(PJLIB_UTIL_LIB) $(PJLIB_LIB)' >> .pjturn-client-$(TARGET_NAME).depend
	echo '$(PJTURN_SRV_EXE): $(PJNATH_LIB) $(PJLIB_UTIL_LIB) $(PJLIB_LIB)' >> .pjturn-srv-$(TARGET_NAME).depend



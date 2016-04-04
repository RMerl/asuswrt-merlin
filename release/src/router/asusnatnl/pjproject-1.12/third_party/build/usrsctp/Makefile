include ../../../build.mak
include ../../../build/common.mak

export LIBDIR := ../../

RULES_MAK := $(PJDIR)/build/rules.mak

export USRSCTP_LIB := ../../lib/libusrsctp-$(TARGET_NAME)$(LIBEXT)

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC). $(CC_INC)../../usrsctp/usrsctplib \
		   $(CC_INC)../../../pjlib/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS) 
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

export USRSCTP_SRCDIR = ../../usrsctp/usrsctplib
export USRSCTP_OBJS = user_environment.o \
			user_mbuf.o \
			user_recv_thread.o \
			user_socket.o \
			netinet/sctp_asconf.o \
			netinet/sctp_auth.o \
			netinet/sctp_bsd_addr.o \
			netinet/sctp_callout.o \
			netinet/sctp_cc_functions.o \
			netinet/sctp_crc32.o \
			netinet/sctp_indata.o \
			netinet/sctp_input.o \
			netinet/sctp_output.o \
			netinet/sctp_pcb.o \
			netinet/sctp_peeloff.o \
			netinet/sctp_sha1.o \
			netinet/sctp_ss_functions.o \
			netinet/sctp_sysctl.o \
			netinet/sctp_timer.o \
			netinet/sctp_userspace.o  \
			netinet/sctp_usrreq.o \
			netinet/sctputil.o \
			netinet6/sctp6_usrreq.o
export USRSCTP_INC = ../../usrsctp/usrsctplib
UMEM_INC = ../../umem

USER_SPACE_CFLAGS=-DSCTP_DEBUG -DSCTP_PROCESS_LEVEL_LOCKS -D__Userspace__
ifneq (,$(findstring android,$(CC)))
    USER_SPACE_CFLAGS+=-DSCTP_SIMPLE_ALLOCATOR
else
    USER_SPACE_CFLAGS+=-I$(UMEM_INC)
endif
ifneq (,$(findstring darwin,$(TARGET_NAME)))
    USER_SPACE_CFLAGS+=-D__APPLE_USE_RFC_2292 -U__APPLE__ -D__Userspace_os_Darwin -DHAVE_SCONN_LEN
else
    USER_SPACE_CFLAGS+=-D__Userspace_os_Linux
endif
#USER_SPACE_CFLAGS+=-DINET -DINET6
USER_SPACE_CFLAGS+=-D_LIB
export USRSCTP_CFLAGS = $(_CFLAGS) $(USER_SPACE_CFLAGS) 
export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := libusrsctp

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: dep depend libusrsctp clean realclean distclean

libusrsctp:
	$(MAKE) -f $(RULES_MAK) APP=USRSCTP app=libusrsctp $(USRSCTP_LIB)

clean print_lib:
	$(MAKE) -f $(RULES_MAK) APP=USRSCTP app=libusrsctp $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.ilbc-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=USRSCTP app=libusrsctp $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=USRSCTP app=libusrsctp $@



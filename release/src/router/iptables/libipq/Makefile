#! /usr/bin/make

EXTRAS+=libipq/libipq.a
#CFLAGS+=-DDEBUG_LIBIPTQ
CFLAGS+=-fPIC

DEVEL_MAN3+=libipq/ipq_create_handle.3 \
	libipq/ipq_destroy_handle.3 \
	libipq/ipq_errstr.3 \
	libipq/ipq_get_msgerr.3 \
	libipq/ipq_get_packet.3 \
	libipq/ipq_message_type.3 \
	libipq/ipq_perror.3 \
	libipq/ipq_read.3 \
	libipq/ipq_set_mode.3 \
	libipq/ipq_set_verdict.3 \
	libipq/libipq.3

DEVEL_LIBS+=libipq/libipq.a

DEVEL_HEADERS+=include/libipq/libipq.h

ifndef TOPLEVEL_INCLUDED
local:
	cd .. && $(MAKE) $(SHARED_LIBS) $(EXTRAS)
else
libipq/libipq.a: libipq/libipq.a(libipq/libipq.o)
endif


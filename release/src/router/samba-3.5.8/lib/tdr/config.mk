[SUBSYSTEM::TDR]
CFLAGS = -Ilib/tdr
PUBLIC_DEPENDENCIES = LIBTALLOC LIBSAMBA-UTIL

TDR_OBJ_FILES = $(libtdrsrcdir)/tdr.o

$(eval $(call proto_header_template,$(libtdrsrcdir)/tdr_proto.h,$(TDR_OBJ_FILES:.o=.c)))

PUBLIC_HEADERS += $(libtdrsrcdir)/tdr.h

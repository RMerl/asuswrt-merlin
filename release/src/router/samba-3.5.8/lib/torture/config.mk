# TORTURE subsystem
[LIBRARY::torture]
PUBLIC_DEPENDENCIES = \
		LIBSAMBA-HOSTCONFIG \
		LIBSAMBA-UTIL \
		LIBSAMBA-ERRORS \
		LIBTALLOC \
		LIBTEVENT
CFLAGS = -I$(libtorturesrcdir) -I$(libtorturesrcdir)/../

torture_VERSION = 0.0.1
torture_SOVERSION = 0

PC_FILES += $(libtorturesrcdir)/torture.pc
torture_OBJ_FILES = $(addprefix $(libtorturesrcdir)/, torture.o subunit.o)

PUBLIC_HEADERS += $(libtorturesrcdir)/torture.h

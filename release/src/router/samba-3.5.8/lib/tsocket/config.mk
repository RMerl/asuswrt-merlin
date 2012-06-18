[SUBSYSTEM::LIBTSOCKET]
PRIVATE_DEPENDENCIES = LIBREPLACE_NETWORK
PUBLIC_DEPENDENCIES = LIBTALLOC LIBTEVENT

LIBTSOCKET_OBJ_FILES = $(addprefix ../lib/tsocket/, \
					tsocket.o \
					tsocket_helpers.o \
					tsocket_bsd.o)

PUBLIC_HEADERS += $(addprefix ../lib/tsocket/, \
				 tsocket.h\
				 tsocket_internal.h)


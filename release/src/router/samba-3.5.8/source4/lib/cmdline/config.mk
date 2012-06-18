[SUBSYSTEM::LIBCMDLINE_CREDENTIALS]
PUBLIC_DEPENDENCIES = CREDENTIALS LIBPOPT

LIBCMDLINE_CREDENTIALS_OBJ_FILES = $(libcmdlinesrcdir)/credentials.o

$(eval $(call proto_header_template,$(libcmdlinesrcdir)/credentials.h,$(LIBCMDLINE_CREDENTIALS_OBJ_FILES:.o=.c)))

[SUBSYSTEM::POPT_SAMBA]
PUBLIC_DEPENDENCIES = LIBPOPT

POPT_SAMBA_OBJ_FILES = $(libcmdlinesrcdir)/popt_common.o

PUBLIC_HEADERS += $(libcmdlinesrcdir)/popt_common.h 

[SUBSYSTEM::POPT_CREDENTIALS]
PUBLIC_DEPENDENCIES = CREDENTIALS LIBCMDLINE_CREDENTIALS LIBPOPT
PRIVATE_DEPENDENCIES = LIBSAMBA-UTIL

POPT_CREDENTIALS_OBJ_FILES = $(libcmdlinesrcdir)/popt_credentials.o

$(eval $(call proto_header_template,$(libcmdlinesrcdir)/popt_credentials.h,$(POPT_CREDENTIALS_OBJ_FILES:.o=.c)))

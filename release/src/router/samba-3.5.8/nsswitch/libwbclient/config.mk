[SUBSYSTEM::LIBWBCLIENT]
PUBLIC_DEPENDENCIES = LIBASYNC_REQ \
		      LIBTEVENT \
		      LIBTALLOC \
		      UTIL_TEVENT

LIBWBCLIENT_OBJ_FILES = $(addprefix $(libwbclientsrcdir)/, wbc_async.o \
								wbc_guid.o \
								wbc_idmap.o \
								wbclient.o \
								wbc_pam.o \
								wbc_pwd.o \
								wbc_sid.o \
								wbc_util.o \
								wb_reqtrans.o )

[SUBSYSTEM::LIBGPO]
PRIVATE_DEPENDENCIES = LIBLDB LIBSAMBA-NET

LIBGPO_OBJ_FILES = ../libgpo/gpo_util.o ../libgpo/gpo_sec.o \
				   ../libgpo/gpext/gpext.o \
				   ../libgpo/gpo_fetch.o ../libgpo/gpo_ini.o \
			$(libgpodir)/ads_convenience.o $(libgpodir)/gpo_filesync.o

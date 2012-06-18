[SUBSYSTEM::LIBCLI_SAMSYNC]
PUBLIC_DEPENDENCIES = \
		LIBCLI_AUTH

LIBCLI_SAMSYNC_OBJ_FILES = $(addprefix $(libclicommonsrcdir)/samsync/, \
		decrypt.o)

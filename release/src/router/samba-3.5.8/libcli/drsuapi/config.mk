[SUBSYSTEM::LIBCLI_DRSUAPI]
PUBLIC_DEPENDENCIES = \
		LIBCLI_AUTH

LIBCLI_DRSUAPI_OBJ_FILES = $(addprefix $(libclicommonsrcdir)/drsuapi/, \
		repl_decrypt.o)

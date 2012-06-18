[SUBSYSTEM::LIBSECURITY_COMMON]
PRIVATE_DEPENDENCIES = TALLOC

LIBSECURITY_COMMON_OBJ_FILES = $(addprefix $(libclicommonsrcdir)/security/, \
					dom_sid.o display_sec.o secace.o secacl.o security_descriptor.o)

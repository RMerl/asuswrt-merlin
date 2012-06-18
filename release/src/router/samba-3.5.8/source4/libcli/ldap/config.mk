[SUBSYSTEM::LIBCLI_LDAP]
PUBLIC_DEPENDENCIES = LIBSAMBA-ERRORS LIBTEVENT LIBPACKET
PRIVATE_DEPENDENCIES = LIBCLI_COMPOSITE samba_socket NDR_SAMR LIBTLS \
		       LIBCLI_LDAP_NDR LIBNDR LP_RESOLVE gensec LIBCLI_LDAP_MESSAGE

LIBCLI_LDAP_OBJ_FILES = $(addprefix $(libclisrcdir)/ldap/, \
					   ldap_client.o ldap_bind.o \
					   ldap_ildap.o ldap_controls.o)
PUBLIC_HEADERS += $(libclisrcdir)/ldap/ldap.h

$(eval $(call proto_header_template,$(libclisrcdir)/ldap/ldap_proto.h,$(LIBCLI_LDAP_OBJ_FILES:.o=.c)))


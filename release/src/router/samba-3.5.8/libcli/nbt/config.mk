[SUBSYSTEM::NDR_NBT_BUF]

NDR_NBT_BUF_OBJ_FILES = $(libclinbtsrcdir)/nbtname.o

$(eval $(call proto_header_template,$(libclinbtsrcdir)/nbtname.h,$(NDR_NBT_BUF_OBJ_FILES:.o=.c)))

[SUBSYSTEM::LIBCLI_NBT]
PUBLIC_DEPENDENCIES = LIBNDR NDR_NBT LIBCLI_COMPOSITE LIBEVENTS \
	NDR_SECURITY samba_socket LIBSAMBA-UTIL

LIBCLI_NBT_OBJ_FILES = $(addprefix $(libclinbtsrcdir)/, \
	lmhosts.o \
	nbtsocket.o \
	namequery.o \
	nameregister.o \
	namerefresh.o \
	namerelease.o)

[BINARY::nmblookup]
INSTALLDIR = BINDIR
PRIVATE_DEPENDENCIES = \
		LIBSAMBA-HOSTCONFIG \
		LIBSAMBA-UTIL \
		LIBCLI_NBT \
		LIBPOPT \
		POPT_SAMBA \
		LIBNETIF \
		LIBCLI_RESOLVE

nmblookup_OBJ_FILES = $(libclinbtsrcdir)/tools/nmblookup.o
MANPAGES += $(libclinbtsrcdir)/man/nmblookup.1

[SUBSYSTEM::LIBCLI_NDR_NETLOGON]
PUBLIC_DEPENDENCIES = LIBNDR  \
	NDR_SECURITY 	

LIBCLI_NDR_NETLOGON_OBJ_FILES = $(addprefix $(libclinbtsrcdir)/../, ndr_netlogon.o)

[SUBSYSTEM::LIBCLI_NETLOGON]
PUBLIC_DEPENDENCIES = LIBSAMBA-UTIL LIBCLI_NDR_NETLOGON

LIBCLI_NETLOGON_OBJ_FILES = $(addprefix $(libclinbtsrcdir)/, \
	../netlogon.o)

[PYTHON::python_netbios]
LIBRARY_REALNAME = samba/netbios.$(SHLIBEXT)
PUBLIC_DEPENDENCIES = LIBCLI_NBT DYNCONFIG LIBSAMBA-HOSTCONFIG

python_netbios_OBJ_FILES = $(libclinbtsrcdir)/pynbt.o


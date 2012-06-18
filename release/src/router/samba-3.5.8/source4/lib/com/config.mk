[SUBSYSTEM::COM]
PRIVATE_DEPENDENCIES = LIBSAMBA-UTIL LIBSAMBA-HOSTCONFIG LIBEVENTS LIBNDR

COM_OBJ_FILES = $(addprefix $(comsrcdir)/, tables.o rot.o main.o)

[SUBSYSTEM::DCOM]
PUBLIC_DEPENDENCIES = COM DCOM_PROXY_DCOM RPC_NDR_REMACT \
					  RPC_NDR_OXIDRESOLVER

DCOM_OBJ_FILES = $(addprefix $(comsrcdir)/dcom/, main.o tables.o)

[MODULE::com_simple]
SUBSYSTEM = COM
INIT_FUNCTION = com_simple_init

com_simple_OBJ_FILES = $(comsrcdir)/classes/simple.o

[PYTHON::pycom]
LIBRARY_REALNAME = samba/com.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = COM

pycom_OBJ_FILES = $(comsrcdir)/pycom.o

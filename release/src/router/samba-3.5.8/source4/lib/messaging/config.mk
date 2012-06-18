[SUBSYSTEM::MESSAGING]
PUBLIC_DEPENDENCIES = \
		LIBSAMBA-UTIL \
		TDB_WRAP \
		NDR_IRPC \
		UNIX_PRIVS \
		UTIL_TDB \
		CLUSTER \
		LIBNDR \
		samba_socket

MESSAGING_OBJ_FILES = $(libmessagingsrcdir)/messaging.o

[PYTHON::python_messaging]
LIBRARY_REALNAME = samba/messaging.$(SHLIBEXT)
PRIVATE_DEPENDENCIES = MESSAGING LIBEVENTS python_irpc pyparam_util

python_messaging_OBJ_FILES = $(libmessagingsrcdir)/pymessaging.o

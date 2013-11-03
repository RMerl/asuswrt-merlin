# Makefile.am for include/atalk/

atalkincludedir = $(includedir)/atalk
BUILT_SOURCES = 
CLEANFILES =

atalkinclude_HEADERS = \
	adouble.h \
	afp.h \
	vfs.h \
	cnid.h \
	logger.h \
	netatalk_conf.h \
	paths.h \
	unicode.h \
	util.h \
	ea.h \
	acl.h \
	unix.h \
	volume.h \
	standards.h \
	bstrlib.h \
	list.h \
	globals.h \
	compat.h \
	uam.h \
	iniparser.h \
	dictionary.h \
	hash.h

noinst_HEADERS = \
	directory.h \
	uuid.h \
	queue.h \
	server_child.h \
	server_ipc.h \
	tdb.h \
	cnid_dbd_private.h \
	cnid_private.h \
	bstradd.h \
	errchk.h \
	ftw.h \
	dsi.h \
	ldapconfig.h \
	fce_api.h

EXTRA_DIST = afp_dtrace.d

if WITH_DTRACE
BUILT_SOURCES += afp_dtrace.h
CLEANFILES += afp_dtrace.h
afp_dtrace.h: $(top_srcdir)/include/atalk/afp_dtrace.d
	$(LIBTOOL) --mode=execute dtrace -o afp_dtrace.h -h -s $(top_srcdir)/include/atalk/afp_dtrace.d
endif

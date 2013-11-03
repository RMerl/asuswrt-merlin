# Makefile.am for libatalk/vfs/

noinst_LTLIBRARIES = libvfs.la

libvfs_la_SOURCES = vfs.c unix.c ea_ad.c ea_sys.c extattr.c

if HAVE_ACLS
libvfs_la_SOURCES += acl.c
endif

## Process this file with automake to produce Makefile.in

man8_MANS	= showmount.man
EXTRA_DIST	= $(man8_MANS)

sbin_PROGRAMS	= showmount
showmount_SOURCES = showmount.c
showmount_LDADD = ../../support/export/libexport.a \
		  ../../support/nfs/libnfs.a \
	  	  ../../support/misc/libmisc.a
showmount_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS) \
		   -I$(top_builddir)/support/export

MAINTAINERCLEANFILES = Makefile.in

## Process this file with automake to produce Makefile.in

man8_MANS	= nfsstat.man
EXTRA_DIST	= $(man8_MANS)

sbin_PROGRAMS	= nfsstat
nfsstat_SOURCES = nfsstat.c
nfsstat_LDADD = ../../support/export/libexport.a \
	      	../../support/nfs/libnfs.a \
		../../support/misc/libmisc.a

MAINTAINERCLEANFILES = Makefile.in

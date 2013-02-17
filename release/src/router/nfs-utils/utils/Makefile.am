## Process this file with automake to produce Makefile.in

OPTDIRS =

if CONFIG_NFSV4
OPTDIRS += idmapd
endif

if CONFIG_GSS
OPTDIRS += gssd
endif

if CONFIG_MOUNT
OPTDIRS += mount
endif

SUBDIRS = \
	exportfs \
	mountd \
	nfsd \
	nfsstat \
	showmount \
	statd \
	$(OPTDIRS)

MAINTAINERCLEANFILES = Makefile.in

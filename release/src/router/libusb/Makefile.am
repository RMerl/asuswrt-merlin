AUTOMAKE_OPTIONS = dist-bzip2
ACLOCAL_AMFLAGS = -I m4
DISTCLEANFILES = ChangeLog libusb.pc
EXTRA_DIST = LICENSE
SUBDIRS = libusb

if BUILD_EXAMPLES
SUBDIRS += examples
endif

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libusb.pc

bin_SCRIPTS = libusb-config

.PHONY: ChangeLog dist-up
ChangeLog:
	git --git-dir $(top_srcdir)/.git log > ChangeLog || touch ChangeLog

dist-hook: ChangeLog

dist-up: dist
	rsync $(distdir).tar.bz2 frs.sourceforge.net:uploads/


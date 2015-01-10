include $(top_srcdir)/config/include-Makefile.am

SHLIBS_DIRS =

if BUILD_LIBUUID
SHLIBS_DIRS += libuuid
endif

if BUILD_LIBBLKID
SHLIBS_DIRS += libblkid
endif

if BUILD_LIBMOUNT
SHLIBS_DIRS += libmount
endif

MAN_DIRS = man/ru

SUBDIRS = \
	include \
	disk-utils \
	fdisk \
	getopt \
	lib \
	$(SHLIBS_DIRS) \
	$(MAN_DIRS) \
	login-utils \
	misc-utils \
	po \
	sys-utils \
	term-utils \
	text-utils \
	tests


RCS_FIND_IGNORE = \( -name SCCS -o -name BitKeeper -o -name .svn -o \
		-name CVS -o -name .pc -o -name .hg -o -name .git \) -prune -o

if BUILD_HWCLOCK
SUBDIRS += hwclock
endif

if BUILD_PARTX
SUBDIRS += partx
endif

if BUILD_MOUNT
SUBDIRS += mount
endif

if BUILD_FSCK
SUBDIRS += fsck
endif

if BUILD_SCHEDUTILS
SUBDIRS += schedutils
endif

AUTOMAKE_OPTIONS = gnu
ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
		.version \
		autogen.sh \
		README.licensing \
		po/update-potfiles \
		tools \
		Documentation

# Arrange so that .tarball-version appears only in the distribution
# tarball, and never in a checked-out repository.
dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version
# Set ChangeLog tag reference.
	sed -i "s/log/log;h=$(VERSION)/" $(distdir)/ChangeLog

distclean-local:
	-find . -name \*~ -o -name \*.orig -o -name \*.rej | xargs rm -f
	rm -rf autom4te.cache


checkincludes:
	@find * $(RCS_FIND_IGNORE) \
		-name '*.[hcS]' -type f -print | sort -u \
		| xargs $(top_srcdir)/tools/checkincludes.pl

checkconfig:
	@find * $(RCS_FIND_IGNORE) \
		-name '*.[hcS]' -type f -print | sort -u \
		| xargs $(top_srcdir)/tools/checkconfig.sh $(top_srcdir)

checkxalloc:
	@ $(top_srcdir)/tools/checkxalloc.sh

ENABLE_ALL = --enable-static-programs --enable-most-builds
DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --disable-use-tty-group $(ENABLE_ALL)


BUILT_SOURCES = .version
.version:
	echo $(VERSION) > $@-t && mv $@-t $@

CHANGELOG_FILE = v$(VERSION)-ChangeLog

$(CHANGELOG_FILE):
	@ if test "x$$PREVIOUS_VERSION" == "x"; then \
		echo "Usage make changelog PREVIOUS_VERSION=<version>"; \
		exit 1; \
	fi
	@ echo "Changes between v$(PREVIOUS_VERSION) and v$(VERSION)" > $(CHANGELOG_FILE)
	@ echo "--------------------------------------------" >> $(CHANGELOG_FILE)
	@ echo >> $(CHANGELOG_FILE)
	$(AM_V_GEN) git log --stat v$(PREVIOUS_VERSION)..HEAD >> $(CHANGELOG_FILE)

changelog: $(CHANGELOG_FILE)



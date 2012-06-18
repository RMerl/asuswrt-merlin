SUBDIRS = m4m po libexif test doc binary contrib

if SHIP_BINARIES
README_W32_XDIST = README-Win32.txt
endif

EXTRA_DIST = @PACKAGE_TARNAME@.spec $(README_W32_XDIST)

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA = libexif.pc
EXTRA_DIST    += libexif.pc.in

noinst_DATA = libexif-uninstalled.pc
EXTRA_DIST += libexif-uninstalled.pc.in

ACLOCAL_AMFLAGS = -I auto-m4 -I m4m

doc_DATA = README AUTHORS NEWS ChangeLog ABOUT-NLS COPYING

#######################################################################
# Help for the maintainer
#

# Simulate something like
#   EXTRA_DIST_IF_EXIST = ChangeLog.cvs
# If present, ship ChangeLog.cvs in source tarball.
# If not present, don't ship it.
dist-hook:
	if test -f $(srcdir)/ChangeLog.cvs; then \
		cp -p $(srcdir)/ChangeLog.cvs $(distdir)/ChangeLog.cvs; \
	fi

.PHONY: cvs-changelog
cvs-changelog: $(srcdir)/ChangeLog.cvs

.PHONY: $(srcdir)/ChangeLog.cvs
$(srcdir)/ChangeLog.cvs:
	if test -f "$(srcdir)/cvs2cl.usermap"; then \
		usermap="--usermap ./cvs2cl.usermap"; \
	else \
		usermap=""; \
	fi; \
	(cd "$(srcdir)" && cvs2cl $${usermap} --utc --domain users.sourceforge.net -f "ChangeLog.cvs")

.PHONY: cvs-tag-release
cvs-tag-release:
	@tag="$$(echo "$(PACKAGE_TARNAME)-$(PACKAGE_VERSION)-release" | sed 's|\.|_|g')"; \
	echo "Are you sure you want to CVS tag \`$${tag}' your source files?"; \
	echo "Press Ctrl-C to abort, Enter to continue."; \
	read; \
	cd "$(srcdir)" && cvs tag "$${tag}"

.PHONY: upload-release
upload-release:
	echo ncftpput upload.sourceforge.net incoming/ $(PACKAGE_TARNAME)-$(PACKAGE_VERSION).*

# End of Makefile.am.

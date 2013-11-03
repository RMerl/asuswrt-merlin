## Process this file with automake to produce Makefile.in
# Copyright (C) 1992, 1999, 2000, 2002 Free Software Foundation, Inc.
#
# This file is part of Libgcrypt.
#
# Libgcrypt is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of
# the License, or (at your option) any later version.
#
# Libgcrypt is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA

ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = dist-bzip2
DISTCHECK_CONFIGURE_FLAGS = --enable-random-daemon \
  --enable-ciphers=arcfour:blowfish:cast5:des:aes:twofish:serpent:rfc2268:seed:camellia

# (A suitable gitlog-to-changelog script can be found in GnuPG master.)
GITLOG_TO_CHANGELOG=gitlog-to-changelog

DIST_SUBDIRS = m4 compat mpi cipher random src tests
SUBDIRS =         compat mpi cipher random src tests
EXTRA_DIST = autogen.sh README.GIT \
	     scripts/ChangeLog-2011 doc/ChangeLog-2011 ChangeLog-2011 \
             m4/ChangeLog-2011 cipher/ChangeLog-2011 src/ChangeLog-2011 \
             random/ChangeLog-2011 tests/ChangeLog-2011 mpi/ChangeLog-2011 \
             scripts/git-log-fix scripts/git-log-footer
DISTCLEANFILES =


# Add all the files listed in "distfiles" files to the distribution,
# apply version number s to some files and create a VERSION file which
# we need for the Prereq: patch file trick.
dist-hook: gen-ChangeLog
	@set -e; \
	 for file in `cd $(top_srcdir); \
	      find mpi -type f -name distfiles`; do \
	    dir=`dirname $$file` ; $(mkinstalldirs) $(distdir)/$$dir ; \
	    for i in distfiles `cat $(top_srcdir)/$$file` ; do \
		ln $(top_srcdir)/$$dir/$$i $(distdir)/$$dir/$$i 2> /dev/null \
		|| cp -p $(top_srcdir)/$$dir/$$i $(distdir)/$$dir/$$i; \
	    done ; \
	done
	@set -e; \
	echo "$(VERSION)" > $(distdir)/VERSION

gen_start_date = 2011-12-01T14:00:00
.PHONY: gen-ChangeLog
gen-ChangeLog:
	if test -d $(top_srcdir)/.git; then                             \
          (cd $(top_srcdir) &&                                          \
            $(GITLOG_TO_CHANGELOG) --append-dot --tear-off              \
            --amend=scripts/git-log-fix                                 \
            --since=$(gen_start_date) ) > $(distdir)/cl-t;              \
          cat $(top_srcdir)/scripts/git-log-footer >> $(distdir)/cl-t;  \
          rm -f $(distdir)/ChangeLog;                                   \
          mv $(distdir)/cl-t $(distdir)/ChangeLog;                      \
        fi


stowinstall:
	$(MAKE) $(AM_MAKEFLAGS) install prefix=/usr/local/stow/libgcrypt

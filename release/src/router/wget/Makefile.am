# Makefile for `Wget' utility
# Copyright (C) 1995, 1996, 1997, 2006, 2007, 2008, 2009, 2010, 2011
# Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Additional permission under GNU GPL version 3 section 7

# If you modify this program, or any covered work, by linking or
# combining it with the OpenSSL project's OpenSSL library (or a
# modified version of that library), containing parts covered by the
# terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
# grants you additional permission to convey the resulting work.
# Corresponding Source for a non-source form of such a combination
# shall include the source code for the parts of OpenSSL used as well
# as that of the covered work.

#
# Version: $(VERSION)
#


# We can't help that installing wget.info leaves /usr/share/info/dir
# around, and we need to prevent uninstallation of the possibly
# previously-existing /etc/wgetrc.
distuninstallcheck_listfiles = find . -type f | \
			       grep -Ev '(/share/info/dir|/etc/wgetrc)$$'

# Search for macros in the m4 subdirectory:
ACLOCAL_AMFLAGS = -I m4

# subdirectories in the distribution
SUBDIRS = lib src po tests util testenv

EXTRA_DIST = ChangeLog.README MAILING-LIST \
             msdos/ChangeLog msdos/config.h msdos/Makefile.DJ \
             msdos/Makefile.WC ABOUT-NLS \
             build-aux/build_info.pl build-aux/git-version-gen .version

CLEANFILES = *~ *.bak $(DISTNAME).tar.gz

BUILT_SOURCES = .version

clean-generic:
	rm -f install-info

.version:
	echo $(VERSION) > $@-t && mv $@-t $@

# Arrange so that .tarball-version appears only in the distribution
# tarball, and never in a checked-out repository.
dist-hook:
	$(AM_V_GEN)echo $(VERSION) > $(distdir)/.tarball-version

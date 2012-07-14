# This file is part of libdaemon.
#
# Copyright 2003-2008 Lennart Poettering
#
# libdaemon is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation, either version 2.1 of the
# License, or (at your option) any later version.
#
# libdaemon is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with libdaemon. If not, see
# <http://www.gnu.org/licenses/>.

dist_html_DATA = README.html style.css
EXTRA_DIST = README.html.in

MAINTAINERCLEANFILES = README.html
CLEANFILES =

doxygen: doxygen.conf
	doxygen doxygen.conf

maintainer-clean-local:
	rm -rf reference

if USE_LYNX
dist_doc_DATA = README
MAINTAINERCLEANFILES += README
README: README.html
	lynx --dump $^ | sed 's,file://localhost/.*/doc/README.html,README,' | sed 's,file://localhost/.*/doc/reference/,reference/,' > $@

CLEANFILES += README
endif

tidy: README.html
	tidy -e < README.html

.PHONY: doxygen tidy

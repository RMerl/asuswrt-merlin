#!/bin/sh
# This file is part of MiniDLNA.
#
# MiniDLNA is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# MiniDLNA is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with MiniDLNA; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.

package="minidlna"

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

cd "$srcdir"
DIE=0

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to compile $package."
	echo "Download the appropriate package for your system,"
	echo "or get the source from one of the GNU ftp sites"
	echo "listed in http://www.gnu.org/order/ftp.html"
	DIE=1
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have automake installed to compile $package."
	echo "Download the appropriate package for your system,"
	echo "or get the source from one of the GNU ftp sites"
	echo "listed in http://www.gnu.org/order/ftp.html"
	DIE=1
}

if test "$DIE" -eq 1; then
	exit 1
fi

echo "Generating configuration files for $package, please wait...."

autoreconf -vfi


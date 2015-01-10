#!/bin/sh

#
# Helps generate autoconf/automake stuff, when code is checked out from SCM.
#
# Copyright (C) 2006-2010 - Karel Zak <kzak@redhat.com>
#

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

THEDIR=`pwd`
cd $srcdir
DIE=0
HAS_GTKDOC=1

test -f mount/mount.c || {
	echo
	echo "You must run this script in the top-level util-linux directory"
	echo
	DIE=1
}

(autopoint --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have autopoint installed to generate util-linux build system."
        echo "The autopoint command is part of the GNU gettext package."
	echo
        DIE=1
}
(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to generate util-linux build system."
	echo
	DIE=1
}
(autoheader --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoheader installed to generate util-linux build system."
	echo "The autoheader command is part of the GNU autoconf package."
	echo
	DIE=1
}
(libtool --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have libtool-2 installed to generate util-linux build system."
	echo
	DIE=1
}
(automake --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have automake installed to generate util-linux build system."
	echo 
	DIE=1
}

ltver=$(libtoolize --version | awk '/^libtoolize/ { print $4 }')
ltver=${ltver:-"none"}
test ${ltver##2.} = "$ltver" && {
	echo "You must have libtool version >= 2.x.x, but you have $ltver."
	DIE=1
}

if test "$DIE" -eq 1; then
	exit 1
fi

echo
echo "Generate build-system by:"
echo "   autopoint:  $(autopoint --version | head -1)"
echo "   aclocal:    $(aclocal --version | head -1)"
echo "   autoconf:   $(autoconf --version | head -1)"
echo "   autoheader: $(autoheader --version | head -1)"
echo "   automake:   $(automake --version | head -1)"
echo "   libtoolize: $(libtoolize --version | head -1)"

rm -rf autom4te.cache

set -e
po/update-potfiles
autopoint --force $AP_OPTS
if ! grep -q datarootdir po/Makefile.in.in; then
	echo autopoint does not honor dataroot variable, patching.
	sed -i -e 's/^datadir *=\(.*\)/datarootdir = @datarootdir@\
datadir = @datadir@/g' po/Makefile.in.in
fi
libtoolize --force $LT_OPTS
aclocal -I m4 $AL_OPTS
autoconf $AC_OPTS
autoheader $AH_OPTS

automake --add-missing $AM_OPTS

cd $THEDIR

echo
echo "Now type '$srcdir/configure' and 'make' to compile."
echo



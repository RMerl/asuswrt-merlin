#!/bin/sh
rm -f ltconfig ltmain.sh config.cache aclocal.m4 config.guess config.sub
# remove the autoconf cache
rm -rf autom4te*.cache
# create a .version file for configure.in
if test ! -f .version; then
   # Building from SVN rather than in a release
   echo 0.0.0-dev > .version
   # for the documentation:
   date +"%e %B %Y" | tr -d '\n' > doc/date.xml
   echo -n 0.0.0-dev > doc/version.xml
fi
set -e
echo -n "libtoolize... "
LIBTOOLIZE=${LIBTOOLIZE:-libtoolize}
if ${LIBTOOLIZE} --help | grep -- --install > /dev/null; then
   ${LIBTOOLIZE} --copy --force --install >/dev/null;
else
   ${LIBTOOLIZE} --copy --force >/dev/null
fi
echo -n "aclocal... "
${ACLOCAL:-aclocal} -I macros
echo -n "autoheader... "
${AUTOHEADER:-autoheader}
echo -n "autoconf... "
${AUTOCONF:-autoconf} -Wall
echo okay.
# remove the autoconf cache
rm -rf autom4te*.cache

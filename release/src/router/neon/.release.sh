#!/bin/sh

set -ex

major=`echo $1 | awk -F. '{print $1;}'`
minor=`echo $1 | awk -F. '{print $2;}'`
release=`echo $1 | awk -F. '{print $3;}'`
version=$1

for f in config.hw; do
in=$f.in
out=$f
sed -e "s/@VERSION@/$version/g" \
    -e "s/@MAJOR@/$major/g" \
    -e "s/@MINOR@/$minor/g" \
    -e "s/@RELEASE@/$release/g" \
    -e "s,@top_srcdir@,`pwd`,g" < $in > $out
done

echo $1 > .version

# for the documentation:
date +"%e %B %Y" | tr -d '\n' > doc/date.xml
echo -n $1 > doc/version.xml

ALL_LINGUAS=`echo po/*.po | sed 's,po/,,g;s,\.po,,g'`

# Try to create a valid Makefile
tmp=`mktemp /tmp/neon-XXXXXX`
sed -e 's,@SET_MAKE@,,;s,@SHELL@,/bin/sh,' \
    -e "s,@top_srcdir@,`pwd`," \
    -e "s,@ALL_LINGUAS@,${ALL_LINGUAS}," \
    < Makefile.in > $tmp
make -f $tmp docs compile-gmo
rm -f $tmp

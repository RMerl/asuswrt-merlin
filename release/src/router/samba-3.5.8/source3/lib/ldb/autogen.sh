#!/bin/sh

rm -rf autom4te.cache
rm -f configure config.h.in

IPATHS="-I libreplace -I lib/replace -I ../libreplace -I ../replace"
IPATHS="$IPATHS -I lib/talloc -I talloc -I ../talloc"
IPATHS="$IPATHS -I lib/tdb -I tdb -I ../tdb"
IPATHS="$IPATHS -I lib/popt -I popt -I ../popt"
autoheader $IPATHS || exit 1
autoconf $IPATHS || exit 1

rm -rf autom4te.cache

echo "Now run ./configure and then make."
exit 0


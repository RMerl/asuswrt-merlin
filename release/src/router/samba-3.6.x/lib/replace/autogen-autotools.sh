#!/bin/sh

rm -rf autom4te.cache
rm -f configure config.h.in

autoheader || exit 1
autoconf || exit 1

rm -rf autom4te.cache

echo "Now run ./configure and then make."
exit 0


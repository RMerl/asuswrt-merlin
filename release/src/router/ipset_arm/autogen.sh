#!/bin/sh -e

aclocal -I m4
autoreconf -fi
rm -Rf autom4te.cache

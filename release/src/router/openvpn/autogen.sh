#!/bin/sh

aclocal -I m4
libtoolize --copy --force
autoheader
automake -a -c --foreign
autoconf
[ ! -f .gitignore ]; touch .gitignore
[ ! -f .gitattributes ]; touch .gitattributes

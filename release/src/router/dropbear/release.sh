#!/bin/sh
VERSION=$(echo '#include "sysoptions.h"\necho DROPBEAR_VERSION' | cpp - | sh)
echo Releasing version "$VERSION" ...
if ! head -n1 CHANGES | grep -q $VERSION ; then
	echo "CHANGES needs updating"
	exit 1
fi

if ! head -n1 debian/changelog | grep -q $VERSION ; then
	echo "debian/changelog needs updating"
	exit 1
fi

head -n1 CHANGES

#sleep 3

RELDIR=$PWD/../dropbear-$VERSION
ARCHIVE=${RELDIR}.tar.bz2
if test -e $RELDIR; then
	echo "$RELDIR exists"
	exit 1
fi

if test -e $ARCHIVE; then
	echo "$ARCHIVE exists"
	exit 1
fi

hg archive "$RELDIR"  || exit 2

(cd "$RELDIR" && autoconf && autoheader) || exit 2

rm -r "$RELDIR/autom4te.cache" || exit 2

rm "$RELDIR/.hgtags"

(cd "$RELDIR/.." && tar cjf $ARCHIVE `basename "$RELDIR"`) || exit 2

ls -l $ARCHIVE
openssl sha -sha256 $ARCHIVE
echo Done to
echo "$ARCHIVE"
echo Sign it with
echo gpg2 --detach-sign -a -u F29C6773 "$ARCHIVE"

#!/bin/sh
# install ldb docs
# tridge@samba.org August 2006

MANDIR="$1"

MAN1="`/bin/ls man/*.1`"
MAN3="`/bin/ls man/*.3`"

if [ -z "$MAN1" ] && [ -z "$MAN3" ]; then
    echo "No manpages have been built"
    exit 0
fi

mkdir -p "$MANDIR/man1" "$MANDIR/man3" 
cp $MAN1 "$MANDIR/man1/" || exit 1
cp $MAN3 "$MANDIR/man3/" || exit 1

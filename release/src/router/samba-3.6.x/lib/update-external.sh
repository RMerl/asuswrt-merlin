#!/bin/sh
# Pull in a new snapshot of external projects that are included in 
# our source tree for users that don't have them installed on their system

TARGETDIR="`dirname $0`"
WORKDIR="`mktemp -d`"

echo "Updating subunit..."
bzr export "$WORKDIR/subunit" lp:subunit 
# Preserve wscript file
cp "$TARGETDIR/subunit/c/wscript" "$WORKDIR/subunit/c/wscript"
rsync -avz --delete "$WORKDIR/subunit/" "$TARGETDIR/subunit/"

echo "Updating testtools..."
bzr export "$WORKDIR/testtools" lp:testtools 
rsync -avz --delete "$WORKDIR/testtools/" "$TARGETDIR/testtools/"

echo "Updating dnspython..."
git clone git://www.dnspython.org/dnspython.git "$WORKDIR/dnspython"
rm -rf "$WORKDIR/dnspython/.git"
rsync -avz --delete "$WORKDIR/dnspython/" "$TARGETDIR/dnspython/"

rm -rf "$WORKDIR"

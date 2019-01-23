#!/bin/bash
# make a release of a Samba library

GPG_USER='Samba Library Distribution Key <samba-bugs@samba.org>'

if [ ! -d ".git" ]; then
	echo "Run this script from the top-level directory in the"
	echo "repository"
	exit 1
fi

if [ $# -lt 1 ]; then
    echo "Usage: librelease.sh <LIBNAMES>"
    exit 1
fi


release_lib() {
    lib="$1"
    srcdir="$2"

    pushd $srcdir

    echo "Releasing library $lib"

    echo "building release tarball"
    tgzname=$(make dist 2>&1 | grep ^Created | cut -d' ' -f2)
    [ -f "$tgzname" ] || {
	echo "Failed to create tarball"
	exit 1
    }
    tarname=$(basename $tgzname .gz)
    echo "Tarball: $tarname"
    gunzip -f $tgzname || exit 1
    [ -f "$tarname" ] || {
	echo "Failed to decompress tarball $tarname"
	exit 1
    }

    tagname=$(basename $tarname .tar | sed s/[\.]/-/g)
    echo "tagging as $tagname"
    git tag -s "$tagname" -m "$lib: tag release $tagname"

    echo "signing"
    rm -f "$tarname.asc"
    gpg -u "$GPG_USER" --detach-sign --armor $tarname || exit 1
    [ -f "$tarname.asc" ] || {
	echo "Failed to create signature $tarname.asc"
	exit 1
    }
    echo "compressing"
    gzip -f -9 $tarname
    [ -f "$tgzname" ] || {
	echo "Failed to compress $tgzname"
	exit 1
    }

    echo "Transferring"
    rsync -Pav $tarname.asc $tgzname master.samba.org:~ftp/pub/$lib/

    popd
}

for lib in $*; do
    case $lib in
	talloc | tdb | tevent)
	    release_lib $lib "lib/$lib"
	    ;;
	ldb)
	    release_lib $lib "source4/lib/$lib"
	    ;;
	*)
	    echo "Unknown library $lib"
	    exit 1
    esac
done

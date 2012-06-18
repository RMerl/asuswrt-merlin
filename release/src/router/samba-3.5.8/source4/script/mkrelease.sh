#!/bin/sh

if [ ! -d ".git" -o `dirname $0` != "./source4/script" ]; then
	echo "Run this script from the top-level directory in the"
	echo "repository as: ./source4/script/mkrelease.sh"
	exit 1
fi

OUTDIR=`mktemp -d samba-XXXXX`
(git archive --format=tar HEAD | (cd $OUTDIR/ && tar xf -))

echo SAMBA_VERSION_IS_GIT_SNAPSHOT=no >> $OUTDIR/source4/VERSION

rm -f $OUTDIR/source4/ldap_server/devdocs/rfc????.txt \
      $OUTDIR/source4/heimdal/lib/wind/rfc????.txt

#Prepare the tarball for a Samba4 release, with some generated files,
#but without Samba3 stuff (to avoid confusion)
( cd $OUTDIR/ || exit 1
 rm -rf README Manifest Read-Manifest-Now Roadmap source3 packaging docs-xml examples swat WHATSNEW.txt MAINTAINERS || exit 1
 cd source4 || exit 1
 ./autogen.sh || exit 1
 ./configure || exit 1
 make dist  || exit 1
) || exit 1

VERSION_FILE=$OUTDIR/source4/version.h
if [ ! -f $VERSION_FILE ]; then
    echo "Cannot find version.h at $VERSION_FILE"
    exit 1;
fi

VERSION=`sed -n 's/^SAMBA_VERSION_STRING=//p' $VERSION_FILE`
echo "Version: $VERSION"
mv $OUTDIR samba-$VERSION || exit 1
tar -cf samba-$VERSION.tar samba-$VERSION || (rm -rf samba-$VERSION; exit 1)
rm -rf samba-$VERSION || exit 1
echo "Now run: "
echo "gpg --detach-sign --armor samba-$VERSION.tar"
echo "gzip samba-$VERSION.tar" 
echo "And then upload "
echo "samba-$VERSION.tar.gz samba-$VERSION.tar.asc" 
echo "to pub/samba/samba4/ on samba.org"




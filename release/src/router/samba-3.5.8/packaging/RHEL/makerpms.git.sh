#!/bin/sh
# Copyright (C) John H Terpstra 1998-2002
# Copyright (C) Gerald (Jerry) Carter 2003
# Copyright (C) Michael Adam 2008

# Script to build RPMs for RHEL from inside a git checkout.

# The following allows environment variables to override the target directories
#   the alternative is to have a file in your home directory calles .rpmmacros
#   containing the following:
#   %_topdir  /home/mylogin/redhat
#
# Note: Under this directory rpm expects to find the same directories
# that are under the /usr/src/redhat directory.

# Set DOCS_TARBALL to the path to a docs release tarball in .tar.bz2 format.
# This tarball should contain the compiled docs with a prefix of "docs/".

# extra options passed to rpmbuild
EXTRA_OPTIONS="$1"

RPMSPECDIR=`rpm --eval %_specdir`
RPMSRCDIR=`rpm --eval %_sourcedir`

# At this point the RPMSPECDIR and RPMSRCDIR variables must have a value!

USERID=`id -u`
GRPID=`id -g`

DIRNAME=$(dirname $0)
TOPDIR=${DIRNAME}/../..
SRCDIR=${TOPDIR}/source3
VERSION_H=${SRCDIR}/include/version.h

SPECFILE="samba.spec"
DOCS="docs.tar.bz2"
RPMVER=`rpm --version | awk '{print $3}'`
RPM="rpmbuild"

##
## Check the RPM version (paranoid)
##
case $RPMVER in
    4*)
       echo "Supported RPM version [$RPMVER]"
       ;;
    *)
       echo "Unknown RPM version: `rpm --version`"
       exit 1
       ;;
esac

##
## determine the samba version and create the SPEC file
##
pushd ${SRCDIR}
./script/mkversion.sh
popd
if [ ! -f ${VERSION_H} ] ; then
	echo "Error creating version.h"
	exit 1
fi

VERSION=`grep "^#define SAMBA_VERSION_OFFICIAL_STRING " ${VERSION_H} | awk '{print $3}'`
vendor_version=`grep "^#define SAMBA_VERSION_VENDOR_SUFFIX " ${VERSION_H} | awk '{print $3}'`
if test "x${vendor_version}"  != "x" ; then
	VERSION="${VERSION}-${vendor_version}"
fi
VERSION=`echo ${VERSION} | sed 's/-/_/g'`
VERSION=`echo ${VERSION} | sed 's/\"//g'`
echo "VERSION: ${VERSION}"
sed -e s/PVERSION/${VERSION}/g \
	-e s/PRELEASE/1/g \
	-e s/PRPMREV//g \
	< ${DIRNAME}/${SPECFILE}.tmpl \
	> ${DIRNAME}/${SPECFILE}


##
## create the tarball
##
pushd ${TOPDIR}
rm -rf ../samba-${VERSION}
git archive --prefix=samba-${VERSION}/ HEAD | (cd .. && tar -xf -)
RC=$?
popd
if [ $RC -ne 0 ]; then
	echo "Build failed!"
	exit 1
fi

if [ "x${DOCS_TARBALL}" != "x" ] && [ -f ${DOCS_TARBALL} ]; then
	cp ${DOCS_TARBALL} /tmp/${DOCS}
	pushd ${TOPDIR}/../samba-${VERSION}
	tar xjf /tmp/${DOCS}
	RC=$?
	rm -f /tmp/${DOCS}
	popd
	if [ $RC -ne 0 ]; then
		echo "Build failed!"
		exit 1
	fi
fi

pushd ${TOPDIR}/..
chown -R ${USERID}.${GRPID} samba-${VERSION}${REVISION}
if [ ! -d samba-${VERSION} ]; then
	ln -s samba-${VERSION}${REVISION} samba-${VERSION} || exit 1
fi
echo -n "Creating samba-${VERSION}.tar.bz2 ... "
tar --exclude=.svn -cf - samba-${VERSION}/. | bzip2 > ${RPMSRCDIR}/samba-${VERSION}.tar.bz2
RC=$?
rm -rf samba-${VERSION}/
popd
echo "Done."
if [ $RC -ne 0 ]; then
        echo "Build failed!"
        exit 1
fi


##
## copy additional source files
##
pushd ${DIRNAME}
chmod 755 setup/filter-requires-samba.sh
tar --exclude=.svn -jcvf - setup > ${RPMSRCDIR}/setup.tar.bz2
cp -p ${SPECFILE} ${RPMSPECDIR}
popd

##
## Build
##
echo "$(basename $0): Getting Ready to build release package"
pushd ${RPMSPECDIR}
${RPM} -ba $EXTRA_OPTIONS $SPECFILE
if [ "x$?" = "x0" ] && [ `arch` = "x86_64" ]; then
    echo "Building 32 bit winbind libs"
    # hi ho, a hacking we will go ...
    ln -sf /lib/libcom_err.so.2 /lib/libcom_err.so
    ln -sf /lib/libuuid.so.1 /lib/libuuid.so
    ln -sf /lib/libkeyutils.so.1 /lib/libkeyutils.so
    ${RPM} -ba --rebuild --target=i386 $SPECFILE
fi

popd

echo "$(basename $0): Done."


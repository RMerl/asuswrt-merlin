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

# extra options passed to rpmbuild
EXTRA_OPTIONS="$1"

RPMSPECDIR=`rpm --eval %_specdir`
RPMSRCDIR=`rpm --eval %_sourcedir`
RPMBUILDDIR=`rpm --eval %_builddir`

# At this point the RPMSPECDIR and RPMSRCDIR variables must have a value!

DIRNAME=$(dirname $0)
TOPDIR=${DIRNAME}/../..

SPECFILE="samba.spec"
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
## Delete the old debuginfo remnants:
##
## At least on RHEL 5.5, we observed broken debuginfo packages
## when either old build directories were still present or old
## debuginfo packages (of samba) were installed.
##
## Remove the debuginfo samba RPMs and old RPM build
## directories, giving the user a 10 second chance to quit.
##

if rpm -qa | grep -q samba-debuginfo || test -n "$(echo ${RPMBUILDDIR}/samba* | grep -v \*)" ; then
	echo "Removing debuginfo remnants to fix debuginfo build:"
	if rpm -qa | grep -q samba-debuginfo ; then
		echo "Uninstalling the samba-debuginfo RPM"
		echo -n "Press Control-C if you want to quit (you have 10 seconds)"
		for count in $(seq 1 10) ; do
			echo -n "."
			sleep 1
		done
		echo
		echo "That was your chance... :-)"
		rpm -e samba-debuginfo
	fi
	if test -n "$(echo ${RPMBUILDDIR}/samba* | grep -v \*)" ; then
		echo "Deleting ${RPMBUILDDIR}/samba*"
		echo -n "Press Control-C if you want to quit (you have 10 seconds)"
		for count in $(seq 1 10) ; do
			echo -n "."
			sleep 1
		done
		echo
		echo "That was your chance... :-)"
		rm -rf ${RPMBUILDDIR}/samba*
	fi
fi

##
## determine the samba version and create the SPEC file
##
${DIRNAME}/makespec.sh
RC=$?
if [ $RC -ne 0 ]; then
	exit ${RC}
fi

RELEASE=$(grep ^Release ${DIRNAME}/${SPECFILE} | sed -e 's/^Release:\ \+//')
VERSION=$(grep ^Version ${DIRNAME}/${SPECFILE} | sed -e 's/^Version:\ \+//')

##
## create the tarball
##
pushd ${TOPDIR}
echo -n "Creating samba-${VERSION}.tar.bz2 ... "
git archive --prefix=samba-${VERSION}/ HEAD | bzip2 > ${RPMSRCDIR}/samba-${VERSION}.tar.bz2
RC=$?
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
## some symlink fixes for building 32bit compat libs
##
if [ `arch` = "x86_64" ]; then
    ln -sf /lib/libcom_err.so.2 /lib/libcom_err.so
    ln -sf /lib/libuuid.so.1 /lib/libuuid.so
fi

##
## Build
##
echo "$(basename $0): Getting Ready to build release package"

pushd ${RPMSPECDIR}
${RPM} -ba $EXTRA_OPTIONS $SPECFILE
popd

echo "$(basename $0): Done."


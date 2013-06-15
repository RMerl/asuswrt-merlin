#!/bin/sh
# Copyright (C) John H Terpstra 1998-2002
# Updated for RPM 3 by Jochen Wiedmann, joe@ispsoft.de
# Changed for a generic tar file rebuild by abartlet@pcug.org.au
# Taken from Red Hat build area by JHT
# Changed by John H Terpstra to build on RH8.1 - should also work for earlier versions jht@samba.org
# Changes from Buchan Milne <bgmilne@cae.co.za>

# The following allows environment variables to override the target directories
#   the alternative is to have a file in your home directory calles .rpmmacros
#   containing the following:
#   %_topdir  /home/mylogin/RPM
#

# rpm --eval should always give a correct answer for this
SPECDIR=`rpm "$@" --eval "%{_specdir}"`
SRCDIR=`rpm "$@" --eval "%{_sourcedir}"`

# At this point the (SPECDIR and) SRCDIR vaiables must have a value!

USERID=`id -u`
GRPID=`id -g`
VERSION='3.0.13'

RPMVER=`rpm --version | awk '{print $3}'`
echo The RPM Version on this machine is: $RPMVER

case $RPMVER in
    2*)
       echo Building for RPM v2.x
       sed -e "s/MANDIR_MACRO/\%\{prefix\}\/man/g" < samba2.spec > samba.spec
       ;;
    3*)
       echo Building for RPM v3.x
       sed -e "s/MANDIR_MACRO/\%\{prefix\}\/man/g" < samba2.spec > samba.spec
       ;;
    4*)
       echo Building for RPM v4.x
       sed -e "s/MANDIR_MACRO/\%\{_mandir\}/g" < samba2.spec > samba.spec
       ;;
    *)
       echo "Unknown RPM version: `rpm --version`"
       exit 1
       ;;
esac

( cd ../../source; if [ -f Makefile ]; then make distclean; fi )
( cd ../../.. ; chown -R ${USERID}.${GRPID} samba-${VERSION} )
echo "Compressing the source as bzip2, may take a while ..."
( cd ../../.. ; tar --exclude=CVS -cjf ${SRCDIR}/samba-${VERSION}.tar.bz2 samba-${VERSION} )

cp -av samba.spec ${SPECDIR}
# cp -a *.patch.bz2 *.xpm.bz2 smb.* samba.xinetd samba.log $SRCDIR
# Prepare to allow straight patches synced from Mandrake cvs:
# Updating of sources and patches can be done more easily and accurately
# by using info in the spec file. It won't work for files that use an rpm
# macro in their name, but that shouldn't be a problem.

SOURCES=`awk '/^Source/ {print $2}' samba.spec |grep -v "%{"`
PATCHES=`awk  '/^Patch/ {print $2}' samba.spec`

for i in $PATCHES $SOURCES;do
	# We have two cases to fix, one where it's bzip2'ed
	# in the spec and not in CVS, one where it's bzip2'ed 
	# in CVS but not in the spec
	[ -e $i ] && cp -av $i $SRCDIR
	i_nobz2=`echo $i|sed -e 's/.bz2$//'`
	i_bz2=$i.bz2
	[ -e $i_nobz2 ] && bzip2 -kf $i_nobz2  && mv -fv $i $SRCDIR
	[ -e $i_bz2 ] && bunzip2 -kf $i_bz2 && mv -fv $i $SRCDIR
done

echo Getting Ready to build release package
cd ${SPECDIR}
rpm -ba -v --clean --rmsource samba.spec $@

echo Done.

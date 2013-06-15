#!/bin/sh

# This file goes through all the necessary steps to build a release package.
# syntax:
#     mkrelease.sh [clean]
#
# You can specify clean to do a make clean before building. Make clean
# will also run configure and generate the required Makefile.
#
# This will build an smbd.noquota, smbd.profile, nmbd.profile and the
# entire package with quota support and acl support.

doclean=""
SGI_ABI=-n32
ISA=-mips3
CC=cc

if [ ! -f ../../source/Makefile ]; then
  doclean="clean"
fi

if [ "$1" = "clean" ] || [ "$1" = "cleanonly" ]; then
  doclean=$1
  shift
fi

export SGI_ABI ISA CC

if [ "$doclean" = "clean" ] || [ "$doclean" = "cleanonly" ]; then
  cd ../../source
  if [ -f Makefile ]; then
    make distclean
  fi
  rm -rf bin/*.profile bin/*.noquota
  cd ../packaging/SGI
  rm -rf bins catman html codepages swat samba.idb samba.spec
  if [ "$doclean" = "cleanonly" ]; then exit 0 ; fi
fi

# create the catman versions of the manual pages
#
if [ "$doclean" = "clean" ]; then
  echo Making manual pages
  ./mkman
  errstat=$?
  if [ $errstat -ne 0 ]; then
    echo "Error $errstat making manual pages\n";
    exit $errstat;
  fi
fi

cd ../../source
if [ "$doclean" = "clean" ]; then
  echo Create SGI specific Makefile
  ./configure --prefix=/usr/samba --sbindir=/usr/samba/bin --mandir=/usr/share/catman --with-acl-support --with-quotas --with-smbwrapper
  errstat=$?
  if [ $errstat -ne 0 ]; then
    echo "Error $errstat creating Makefile\n";
    exit $errstat;
  fi
fi


# build the sources
#
echo Making binaries

echo "=====================  Making Profile versions ======================="
make clean
make headers
make -P "CFLAGS=-O -g3 -woff 1188 -D WITH_PROFILE" bin/smbd bin/nmbd
errstat=$?
if [ $errstat -ne 0 ]; then
  echo "Error $errstat building profile sources\n";
  exit $errstat;
fi
mv  bin/smbd bin/smbd.profile
mv  bin/nmbd bin/nmbd.profile

echo "=====================  Making No Quota versions ======================="
make clean
make headers
make -P "CFLAGS=-O -g3 -woff 1188 -D QUOTAOBJS=smbd/noquotas.o" bin/smbd
errstat=$?
if [ $errstat -ne 0 ]; then
  echo "Error $errstat building noquota sources\n";
  exit $errstat;
fi
mv  bin/smbd bin/smbd.noquota

echo "=====================  Making Regular versions ======================="
make -P "CFLAGS=-O -g3 -woff 1188" all libsmbclient
errstat=$?
if [ $errstat -ne 0 ]; then
  echo "Error $errstat building sources\n";
  exit $errstat;
fi

cd ../packaging/SGI

# generate the packages
#
echo Generating Inst Packages
./spec.pl			# create the samba.spec file
errstat=$?
if [ $errstat -ne 0 ]; then
  echo "Error $errstat creating samba.spec\n";
  exit $errstat;
fi

./idb.pl			# create the samba.idb file
errstat=$?
if [ $errstat -ne 0 ]; then
  echo "Error $errstat creating samba.idb\n";
  exit $errstat;
fi
sort +4 samba.idb > xxx
mv xxx samba.idb

if [ ! -d bins ]; then
   mkdir bins
fi

# do the packaging
/usr/sbin/gendist -rbase / -sbase ../.. -idb samba.idb -spec samba.spec -dist ./bins -all


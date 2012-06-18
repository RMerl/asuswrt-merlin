#!/bin/sh

INSTALLPERMS=$1
BASEDIR=$2
BINDIR=$3
LIBDIR=$4
VARDIR=$5
shift
shift
shift
shift
shift

for d in $BASEDIR $BINDIR $LIBDIR $VARDIR; do
if [ ! -d $d ]; then
mkdir $d
if [ ! -d $d ]; then
  echo Failed to make directory $d
  exit 1
fi
fi
done


for p in $*; do
 p2=`basename $p`
 echo Installing $p as $BINDIR/$p2
 if [ -f $BINDIR/$p2 ]; then
   rm -f $BINDIR/$p2.old
   mv $BINDIR/$p2 $BINDIR/$p2.old
 fi
 cp $p $BINDIR/
 chmod $INSTALLPERMS $BINDIR/$p2

 # this is a special case, mount needs this in a specific location
 if [ $p2 = smbmount ]; then
   ln -sf $BINDIR/$p2 /sbin/mount.smbfs
 fi
done


cat << EOF
======================================================================
The binaries are installed. You may restore the old binaries (if there
were any) using the command "make revert". You may uninstall the binaries
using the command "make uninstallbin" or "make uninstall" to uninstall
binaries, man pages and shell scripts.
======================================================================
EOF

exit 0

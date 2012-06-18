#!/bin/sh

INSTALLPERMS=$1
DESTDIR=$2
prefix=`echo $3 | sed 's/\/\//\//g'`
LIBDIR=`echo $4 | sed 's/\/\//\//g'`
shift
shift
shift
shift

for d in $prefix $LIBDIR; do
if [ ! -d $DESTDIR/$d ]; then
mkdir $DESTDIR/$d
if [ ! -d $DESTDIR/$d ]; then
  echo Failed to make directory $DESTDIR/$d
  exit 1
fi
fi
done

# We expect the last component of LIBDIR to be the module type, eg. idmap,
# pdb. By stripping this from the installation name, you can have multiple
# modules of the same name but different types by creating eg. idmap_foo
# and pdb_foo. This makes the most sense for idmap and pdb module, where
# they need to be consistent.
mtype=`basename $LIBDIR`

for p in $*; do
 p2=`basename $p`
 name=`echo $p2 | sed -es/${mtype}_//`
 echo Preserving old module as $DESTDIR/$LIBDIR/$name.old
 if [ -f $DESTDIR/$LIBDIR/$name ]; then
   rm -f $DESTDIR/$LIBDIR/$name.old
   mv $DESTDIR/$LIBDIR/$name $DESTDIR/$LIBDIR/$name.old
 fi
 echo Installing $p as $DESTDIR/$LIBDIR/$name
 cp -f $p $DESTDIR/$LIBDIR/$name
 chmod $INSTALLPERMS $DESTDIR/$LIBDIR/$name
done

exit 0

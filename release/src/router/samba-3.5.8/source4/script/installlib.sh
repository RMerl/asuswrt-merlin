#!/bin/sh

LIBDIR=$1
SHLIBEXT=$2

shift
shift

for p in $*; do
 p2=`basename $p`
 lnname=`echo $p2 | sed -e "s/\.$SHLIBEXT.*/.$SHLIBEXT/"`
 echo Installing $p as $LIBDIR/$p2
 if [ -f $LIBDIR/$p2 ]; then
   rm -f $LIBDIR/$p2.old
   mv $LIBDIR/$p2 $LIBDIR/$p2.old
 fi
 cp $p $LIBDIR/
 if [ $p2 != $lnname ]; then
  ln -sf $p2 $LIBDIR/$lnname
 fi
done

cat << EOF
======================================================================
The shared libraries are installed. You may restore the old libraries (if there
were any) using the command "make revert". You may uninstall the libraries
using the command "make uninstalllib" or "make uninstall" to uninstall
binaries, man pages and shell scripts.
======================================================================
EOF

exit 0

#!/bin/sh
# based on uninstallbin.sh
# 4 July 96 Dan.Shearer@UniSA.edu.au   

LIBDIR=$1
shift

if [ ! -d $LIBDIR ]; then
  echo Directory $LIBDIR does not exist!
  echo Do a "make installbin" or "make install" first.
  exit 1
fi

for p in $*; do
  p2=`basename $p`
  if [ -f $LIBDIR/$p2 ]; then
    echo Removing $LIBDIR/$p2
    rm -f $LIBDIR/$p2
    if [ -f $LIBDIR/$p2 ]; then
      echo Cannot remove $LIBDIR/$p2 ... does $USER have privileges?
    fi
  fi
done


cat << EOF
======================================================================
The shared libraries have been uninstalled. You may restore the libraries using
the command "make installlib" or "make install" to install binaries, 
man pages, modules and shell scripts. You can restore a previous
version of the libraries (if there were any) using "make revert".
======================================================================
EOF

exit 0

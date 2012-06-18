#!/bin/sh

CPDIR=$1
shift

if [ ! -d $CPDIR ]; then
  echo Directory $CPDIR does not exist!
  echo Do a "make installcp" or "make install" first.
  exit 1
fi

for p in $*; do
  if [ ! -f $CPDIR/codepage.$p ]; then
    echo $CPDIR/codepage.$p does not exist!
  else
    echo Removing $CPDIR/codepage.$p
    rm -f $CPDIR/codepage.$p
    if [ -f $CPDIR/codepage.$p ]; then
      echo Cannot remove $CPDIR/codepage.$p... does $USER have privileges?
    fi
  fi
done

cat << EOF
======================================================================
The code pages have been uninstalled. You may reinstall them using
the command "make installcp" or "make install" to install binaries,
man pages, shell scripts and code pages. You may recover a previous version 
(if any with "make revert").
======================================================================
EOF

exit 0

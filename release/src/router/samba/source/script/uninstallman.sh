#!/bin/sh
#4 July 96 Dan.Shearer@UniSA.edu.au

MANDIR=$1
SRCDIR=$2

echo Uninstalling man pages from $MANDIR

for sect in 1 5 7 8 ; do
  for m in $MANDIR/man$sect ; do
    for s in $SRCDIR/../docs/manpages/*$sect; do
      FNAME=$m/`basename $s`
      if test -f $FNAME; then
        echo Deleting $FNAME
        rm -f $FNAME 
        test -f $FNAME && echo Cannot remove $FNAME... does $USER have privileges?   
      fi
    done
  done
done

cat << EOF
======================================================================
The man pages have been uninstalled. You may install them again using 
the command "make installman" or make "install" to install binaries,
man pages and shell scripts.
======================================================================
EOF
exit 0

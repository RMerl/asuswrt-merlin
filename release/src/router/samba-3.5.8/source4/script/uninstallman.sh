#!/bin/sh
# 4 July 96 Dan.Shearer@UniSA.edu.au
# Updated for Samba4 by Jelmer Vernooij

MANDIR=$1
shift 1
MANPAGES=$*

for I in $MANPAGES
do
	SECTION=`echo -n $I | sed "s/.*\(.\)$/\1/"`
	FNAME=$MANDIR/man$SECTION/$I
	if test -f $FNAME; then
	  echo Deleting $FNAME
	  rm -f $FNAME 
	  test -f $FNAME && echo Cannot remove $FNAME... does $USER have privileges?   
    fi
done

cat << EOF
======================================================================
The man pages have been uninstalled. You may install them again using 
the command "make installman" or make "install" to install binaries,
man pages and shell scripts.
======================================================================
EOF
exit 0

#!/bin/sh
#5 July 96 Dan.Shearer@unisa.edu.au  removed hardcoded values

MANDIR=$1
SRCDIR=$2/
if [ $# -ge 3 ] ; then
  GROFF=$3                    # sh cmd line, including options 
fi

echo Installing man pages in $MANDIR

for d in $MANDIR $MANDIR/man1 $MANDIR/man5 $MANDIR/man7 $MANDIR/man8; do
if [ ! -d $d ]; then
mkdir $d
if [ ! -d $d ]; then
  echo Failed to make directory $d, does $USER have privileges?
  exit 1
fi
fi
done

for sect in 1 5 7 8 ; do
  for m in $MANDIR/man$sect ; do
    for s in $SRCDIR../docs/manpages/*$sect; do
      FNAME=$m/`basename $s`
 
       # Test for writability.  Involves 
       # blowing away existing files.
 
       if (rm -f $FNAME && touch $FNAME); then
         rm $FNAME
         if [ "x$GROFF" = x ] ; then
           cp $s $m              # Copy raw nroff 
         else
           echo "\t$FNAME"       # groff'ing can be slow, give the user
                                 #   a warm fuzzy.
           $GROFF $s > $FNAME    # Process nroff, because man(1) (on
                                 #   this system) doesn't .
         fi
         chmod 0644 $FNAME
       else
         echo Cannot create $FNAME... does $USER have privileges?
       fi
    done
  done
done

cat << EOF
======================================================================
The man pages have been installed. You may uninstall them using the command
the command "make uninstallman" or make "uninstall" to uninstall binaries,
man pages and shell scripts.
======================================================================
EOF

exit 0


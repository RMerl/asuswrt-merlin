#!/bin/sh
#fist version March 2002, Herb  Lewis

DATDIR=$1
SRCDIR=$2/

echo Installing dat files in $DATDIR

for f in $SRCDIR/../codepages/*.dat; do
	FNAME=$DATDIR/`basename $f`
	echo $FNAME
	cp $f $FNAME || echo Cannot install $FNAME. Does $USER have privileges?
	chmod 0644 $FNAME
done

cat << EOF
======================================================================
The dat files have been installed. 
======================================================================
EOF

exit 0


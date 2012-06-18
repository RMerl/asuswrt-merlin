#!/bin/sh

SWATDIR=$1
SRCDIR=$2

echo Installing swat files in $SWATDIR

cd $SRCDIR/../swat || exit 1

mkdir -p $SWATDIR || exit 1

installdir() {
    for f in $*; do
	dname=`dirname $f`
	echo "Installing $f in $dname"
	test -d $SWATDIR/$dname || mkdir -p $SWATDIR/$dname || exit 1
	cp $f $SWATDIR/$dname/ || exit 1
	chmod 0644 $SWATDIR/$f || exit 1
    done
}

installdir `find . -name '*.js'`
installdir `find . -name '*.esp'`
installdir `find . -name '*.css'`
installdir `find . -name '*.png'`
installdir `find . -name '*.ico'`
installdir `find . -name '*.gif'`
installdir `find . -name '*.ejs'`

cat << EOF
======================================================================
The swat files have been installed. 
======================================================================
EOF

exit 0


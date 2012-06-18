#!/bin/sh

MANDIR=$1
shift 1
MANPAGES=$*

for I in $MANPAGES
do
	SECTION=`echo -n $I | sed "s/.*\(.\)$/\1/"`
	DIR="$MANDIR/man$SECTION"
	if [ ! -d "$DIR" ]
	then
		mkdir "$DIR"
	fi

	BASE=`basename $I`
	
	echo "Installing manpage \"$BASE\" in $DIR"
	cp $I $DIR
done

cat << EOF
======================================================================
The man pages have been installed. You may uninstall them using the command
the command "make uninstallman" or make "uninstall" to uninstall binaries,
man pages and shell scripts.
======================================================================
EOF

exit 0

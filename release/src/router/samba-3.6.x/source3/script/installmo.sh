#!/bin/sh

DESTDIR=$1
LOCALEDIR=`echo $2 | sed 's/\/\//\//g'`
SRCDIR=$3/
MSGFMT=msgfmt

case $0 in
	*uninstall*)
		if test ! -d "$DESTDIR/$LOCALEDIR"; then
			echo "Directory $DESTDIR/$LOCALEDIR doesn't exist!"
			echo "Do a \"make installmo\" or \"make install\" first."
			exit 1
		fi
		mode='uninstall'
	;;
	*)
		mode='install'
	;;
esac

for dir in $SRCDIR/locale/*; do
	MODULE=`basename $dir`
	for f in $SRCDIR/locale/$MODULE/*.po; do
		BASE=`basename $f`
		LANGUAGE=`echo $BASE | sed 's/\.po//g'`
		if test "$LANGUAGE" = '*'; then
			echo "No .po file exists!"
			exit 0
		fi
		FNAME="$DESTDIR/$LOCALEDIR/$LANGUAGE/LC_MESSAGES/$MODULE.mo"
		if test ! -d "$DESTDIR/$LOCALEDIR/$LANGUAGE/LC_MESSAGES/"; then
			mkdir -p "$DESTDIR/$LOCALEDIR/$LANGUAGE/LC_MESSAGES/"
		fi
		if test "$mode" = 'install'; then
			echo "Installing $f as $FNAME"
			touch "$FNAME"
			$MSGFMT -f -o "$FNAME" "$f"
			if test ! -f "$FNAME"; then
				echo "Cannot install $FNAME. Does $USER have privileges?"
				exit 1
			fi
			chmod 0644 "$FNAME"
		elif test "$mode" = 'uninstall'; then
			echo "removing $FNAME"
			rm -f "$FNAME"
			if test -f "$FNAME"; then
				echo "Cannot remove $FNAME. Does $USER have privileges?"
				exit 1
			fi
		else
			echo "Unknown mode $mode. script called as $0."
			exit 1
		fi
	done
	if test "$mode" = 'install'; then
		cat << EOF
==============================================================
MO files for $MODULE are installed.
==============================================================
EOF
	else
		cat << EOF
==============================================================
MO files for $MODULE are removed.
==============================================================
EOF
	fi
done

if test "$mode" = 'install'; then
	cat << EOF
==============================================================
All MO files for Samba are installed. You can use "make uninstall"
or "make uninstallmo" to remove them.
==============================================================
EOF
else
	cat << EOF
==============================================================
All MO files for Samba are removed. you can use "make install"
or "make installmo" to install them.
==============================================================
EOF
fi

exit 0

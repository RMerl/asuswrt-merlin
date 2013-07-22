#!/bin/bash

#
# This script checks for HAVE_ and ENABLE_ macros which are
# not included in config.h.in
#
# Usage:   checkconfig.sh <top_srcdir> <srcfile> [<srcfile> ...]
#
# Copyright (C) 2007 Matthias Koenig <mkoenig@suse.de>
# Copyright (C) 2008 Karel Zak <kzak@redhat.com>
#


function die() {
	echo "error: $1"
	exit 1
}

srcdir=$1
config="$srcdir/config.h.in"

[ -d "$srcdir" ] || die "$srcdir: not such directory."
[ -f "$config" ] || die "$config: not such file."

shift

while (( "$#" )); do
	srcfile=$1
	shift

	[ ! -f "$srcfile" ] && continue;

	# Note that we use HAVE_ macros since util-linux-ng-2.14. The
	# previous version also have used ENABLE_ too.
	#
	# ENABLE_ and HAVE_ macros shouldn't be used for any other pupose that
	# for config/build options.
	#
	DEFINES=$(sed -n -e 's/.*[ \t(]\+\(HAVE_[[:alnum:]]\+[^ \t);]*\).*/\1/p' \
			 -e 's/.*[ \t(]\+\(ENABLE_[[:alnum:]]\+[^ \t);]*\).*/\1/p' \
                         $srcfile | sort -u)
	[ -z "$DEFINES" ] && continue

	for d in $DEFINES; do
		case $d in
		HAVE_CONFIG_H) continue
		   ;;
		*) grep -q "$d\( \|\>\)" $config || \
		     echo $(echo $srcfile | sed 's:\\'$srcdir/'::') ": $d"
	           ;;
		esac
	done
done

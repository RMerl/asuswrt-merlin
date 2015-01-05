#!/bin/bash

#
# Copyright (C) 2007 Karel Zak <kzak@redhat.com>
#
# This file is part of util-linux.
#
# This file is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This file is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#

TS_TOPDIR=$(cd $(dirname $0) && pwd)
SUBTESTS=
OPTS=

while [ -n "$1" ]; do
	case "$1" in
	--force)
		OPTS="$OPTS --force"
		;;
	--fake)
		OPTS="$OPTS --fake"
		;;
	--memcheck)
		OPTS="$OPTS --memcheck"
		;;
	--*)
		echo "Unknown option $1"
		echo "Usage: run [--fake] [--force] [<component> ...]"
		exit 1
		;;

	*)
		SUBTESTS="$SUBTESTS $1"
		;;
	esac
	shift
done

if [ -n "$SUBTESTS" ]; then
	# selected tests only
	for s in $SUBTESTS; do
		if [ -d "$TS_TOPDIR/ts/$s" ]; then
			co=$(find $TS_TOPDIR/ts/$s -type f -perm /a+x -regex ".*/[^\.~]*" |  sort)
			comps="$comps $co"
		else
			echo "Unknown test component '$s'"
			exit 1
		fi
	done
else
	# all tests
	comps=$(find $TS_TOPDIR/ts/ -type f -perm /a+x -regex ".*/[^\.~]*" |  sort)
fi

echo
echo "------------------ Utils-linux regression tests ------------------"
echo
echo "                    For development purpose only.                    "
echo "                 Don't execute on production system!                 "
echo

res=0
count=0
for ts in $comps; do
	$ts "$OPTS"
	res=$(( $res + $? ))
	count=$(( $count + 1 ))
done

echo
echo "---------------------------------------------------------------------"
if [ $res -eq 0 ]; then
	echo "  All $count tests PASSED"
	res=0
else
	echo "  $res tests of $count FAILED"
	res=1
fi
echo "---------------------------------------------------------------------"
exit $res

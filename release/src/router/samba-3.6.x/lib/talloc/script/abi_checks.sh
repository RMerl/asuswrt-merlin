#!/bin/sh

#
# abi_checks.sh - check for possible abi changes
#
# Copyright (C) 2009 Michael Adam <obnox@samba.org>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 3 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <http://www.gnu.org/licenses/>.
#

#
# USAGE: abi_checks.sh LIBNAME header1 [header2 ...]
#
# This script creates symbol and signature lists from the provided header
# files with the aid of the mksyms.sh and mksigs.pl scripts (saved as
# $LIBNAME.exports.check and $LIBNAME.sigatures.check). It then compares
# the resulting files with the files $LIBNAME.exports and $LIBNME.signatures
# which it expects to find in the current directory.
#

LANG=C; export LANG
LC_ALL=C; export LC_ALL
LC_COLLATE=C; export LC_COLLATE

exit_status=0
script=$0
dir_name=$(dirname ${script})

if test x"$1" = "x" ; then
	echo "USAGE: ${script} libname header [header ...]"
	exit 1
fi

libname="$1"
shift

if test x"$1" = "x" ; then
	echo "USAGE: ${script} libname header [header ...]"
	exit 1
fi

headers="$*"

exports_file=${libname}.exports
exports_file_check=${exports_file}.check
signatures_file=${libname}.signatures
signatures_file_check=${signatures_file}.check


${dir_name}/mksyms.sh awk ${exports_file_check} ${headers} 2>&1 > /dev/null
cat ${headers} | ${dir_name}/mksigs.pl | sort| uniq > ${signatures_file_check} 2> /dev/null

diff -u ${exports_file} ${exports_file_check}
if test "x$?" != "x0" ; then
	echo "WARNING: possible ABI change detected in exports!"
	let exit_status++
else
	echo "exports check: OK"
fi

diff -u ${signatures_file} ${signatures_file_check}
if test "x$?" != "x0" ; then
	echo "WARNING: possible ABI change detected in signatures!"
	let exit_status++
else
	echo "signatures check: OK"
fi

exit $exit_status

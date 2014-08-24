#!/bin/sh

#   Copyright (C) 2003, 2005, 2007 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

#
# gcore.sh
# Script to generate a core file of a running program.
# It starts up gdb, attaches to the given PID and invokes the gcore command.
#

if [ "$#" -eq "0" ]
then
    echo "usage:  gcore [-o filename] pid"
    exit 2
fi

# Need to check for -o option, but set default basename to "core".
name=core

if [ "$1" = "-o" ]
then
    if [ "$#" -lt "3" ]
    then
	# Not enough arguments.
	echo "usage:  gcore [-o filename] pid"
	exit 2
    fi
    name=$2

    # Shift over to start of pid list
    shift; shift
fi

# Create a temporary file.  Use mktemp if available, but cope if it is not.
tmpfile=`mktemp ${name}.XXXXXX 2>/dev/null` || {
  tmpfile=${name}.$$
  if test -e $tmpfile; then
    echo "Could not create temporary file $tmpfile"
    exit 1
  fi
  touch $tmpfile
}
trap "rm -f $tmpfile" EXIT

# Initialise return code.
rc=0

# Loop through pids
for pid in $*
do
	# Write gdb script for pid $pid.  
	cat >>$tmpfile <<EOF
attach $pid
gcore $name.$pid
detach
quit
EOF

	gdb -x $tmpfile -batch

	if [ -r $name.$pid ] ; then 
	    rc=0
	else
	    echo gcore: failed to create $name.$pid
	    rc=1
	    break
	fi


done

exit $rc

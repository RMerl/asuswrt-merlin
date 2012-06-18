#!/bin/sh

while ( test -n "$1" ); do
	if [ ! -d $1 ]; then
		mkdir -p $1
	fi

	if [ ! -d $1 ]; then
		echo Failed to make directory $1
		exit 1
	fi

	shift;
done




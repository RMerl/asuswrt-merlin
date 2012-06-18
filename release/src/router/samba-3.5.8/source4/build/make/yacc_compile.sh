#!/bin/sh

YACC="$1"
SRC="$2"
DEST="$3"

dir=`dirname $SRC`
file=`basename $SRC`
base=`basename $SRC .y`
if [ -z "$YACC" ]; then
	# if $DEST is more recent than $SRC, we can just touch
	# otherwise we touch but print out warnings
	if [ -r $DEST ]; then
		if [ x`find $SRC -newer $DEST -print` = x$SRC ]; then
			echo "warning: yacc not found - cannot generate $SRC => $DEST" >&2
			echo "warning: yacc not found - only updating the timestamp of $DEST" >&2
		fi
		touch $DEST;
		exit;
	fi
	echo "error: yacc not found - cannot generate $SRC => $DEST" >&2
	exit 1;
fi
# if $DEST is more recent than $SRC, we can just touch
if [ -r $DEST ]; then
	if [ x`find $SRC -newer $DEST -print` != x$SRC ]; then
		touch $DEST;
		exit;
	fi
fi
TOP=`pwd`
echo "info: running $YACC -d $file"
if cd $dir && $YACC -d $file; then
	if [ -r y.tab.h -a -r y.tab.c ];then
		echo "info: move y.tab.h to $base.h"
		sed -e "/^#/!b" -e "s|y\.tab\.h|$SRC|" -e "s|\"$base.y|\"$SRC|"  y.tab.h > $base.h
		echo "info: move y.tab.c to $base.c"
		sed -e "s|y\.tab\.c|$SRC|" -e "s|\"$base.y|\"$SRC|" y.tab.c > $base.c
		rm -f y.tab.c y.tab.h
	elif [ ! -r $base.h -a ! -r $base.c]; then
		echo "$base.h nor $base.c generated."
		exit 1
	fi
fi
cd $TOP

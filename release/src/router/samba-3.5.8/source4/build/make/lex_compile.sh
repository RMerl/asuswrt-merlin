#!/bin/sh

LEX="$1"
SRC="$2"
DEST="$3"
shift 3
ARGS="$*"

dir=`dirname $SRC`
file=`basename $SRC`
base=`basename $SRC .l`
if [ -z "$LEX" ]; then
	# if $DEST is more recent than $SRC, we can just touch
	# otherwise we touch but print out warnings
	if [ -r $DEST ]; then
		if [ x`find $SRC -newer $DEST -print` = x$SRC ]; then
			echo "warning: lex not found - cannot generate $SRC => $DEST" >&2
			echo "warning: lex not found - only updating the timestamp of $DEST" >&2
		fi
		touch $DEST;
		exit;
	fi
	echo "error: lex not found - cannot generate $SRC => $DEST" >&2
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
echo "info: running $LEX $ARGS $file"
if cd $dir && $LEX $ARGS $file; then
	if [ -r lex.yy.c ];then
	        # we must guarantee that config.h comes first
		echo "info: move lex.yy.c to $base.c"
	        echo "#include \"config.h\"" > $base.c
		sed -e "s|lex\.yy\.c|$DEST|" lex.yy.c >> $base.c
		rm -f $base.yy.c
	elif [ -r $base.yy.c ];then
	        # we must guarantee that config.h comes first
		echo "info: move $base.yy.c to $base.c"
	        echo "#include \"config.h\"" > $base.c
		sed -e "s|$base\.yy\.c|$DEST|" $base.yy.c >> $base.c
		rm -f $base.yy.c
	elif [ -r $base.c ];then
	        # we must guarantee that config.h comes first
		echo "info: add #include \"config.h\" to $base.c"
		mv $base.c $base.c.tmp
	        echo "#include \"config.h\"" > $base.c
		sed -e "s|$base\.yy\.c|$DEST|" $base.c.tmp >> $base.c
		rm -f $base.c.tmp
	elif [ ! -r base.c ]; then
		echo "$base.c nor $base.yy.c nor lex.yy.c generated."
		exit 1
	fi
fi
cd $TOP

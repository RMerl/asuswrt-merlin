#!/bin/sh

if [ "$1" = "--full" ]; then
	FULL=1
	shift 1
else
	FULL=0
fi

ARGS="--includedir=../librpc/idl --outputdir $PIDL_OUTPUTDIR --header --ndr-parser --client --samba3-ndr-server $PIDL_ARGS --"
IDL_FILES="$*"

oldpwd=`pwd`
cd ${srcdir}

[ -d $PIDL_OUTPUTDIR ] || mkdir -p $PIDL_OUTPUTDIR || exit 1

PIDL="$PIDL $ARGS"

if [ $FULL = 1 ]; then
	echo "Rebuilding all idl files"
	$PIDL $IDL_FILES || exit 1
	exit 0
fi

##
## Find newer files rather than rebuild all of them
##

list=""
for f in ${IDL_FILES}; do
        b=`basename $f .idl`
	outfiles="$b.h ndr_$b.h srv_$b.c"
	outfiles="$outfiles ndr_$b.c srv_$b.h"

	for o in $outfiles; do
	    [ -f $PIDL_OUTPUTDIR/$o ] || {
		list="$list $f"
		break
	    }
	    test "`find $f -newer $PIDL_OUTPUTDIR/$o`" != "" && {
		list="$list $f"
		break
	    }
	done
done

##
## generate the ndr stubs
##

if [ "x$list" != x ]; then
	# echo "${PIDL} ${list}"
	$PIDL $list || exit 1
fi

cd ${oldpwd}

exit 0


#!/bin/sh

if [ "$1" = "--full" ]; then
	FULL=1
	shift 1
else
	FULL=0
fi

ARGS="--outputdir $PIDL_OUTPUTDIR --header --ndr-parser --samba3-ndr-server --samba3-ndr-client --server --client --python --dcom-proxy --com-header $PIDL_ARGS --"
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
	basename=`basename $f .idl`
	ndr="$PIDL_OUTPUTDIR/py_$basename.c"

	if [ -f $ndr ]; then
		if [ "x`find $f -newer $ndr -print`" = "x$f" ]; then
			list="$list $f"
		fi
	else 
		list="$list $f"
	fi
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

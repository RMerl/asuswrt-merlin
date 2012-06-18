#!/bin/sh

FULLBUILD=$1
OUTDIR=$2
shift 2
IDL_FILES="$*"

[ -d $OUTDIR ] || mkdir -p $OUTDIR || exit 1

PIDL="$PIDL --outputdir $OUTDIR --header --ndr-parser --server --client --python --dcom-proxy --com-header --includedir ../librpc/idl -- "

if [ x$FULLBUILD = xFULL ]; then
      echo Rebuilding all idl files in $IDLDIR
      $PIDL $IDL_FILES || exit 1
      exit 0
fi

list=""

for f in $IDL_FILES ; do
    basename=`basename $f .idl`
    ndr="$OUTDIR/ndr_$basename.c"
    # blergh - most shells don't have the -nt function
    if [ -f $ndr ]; then
	if [ x`find $f -newer $ndr -print` = x$f ]; then
	    list="$list $f"
	fi
    else 
        list="$list $f"
    fi
done

if [ "x$list" != x ]; then
    $PIDL $list || exit 1
fi

exit 0

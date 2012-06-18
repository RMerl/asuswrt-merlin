#!/bin/sh

#set -x

NM=mipsel-uclibc-nm
LD=mipsel-uclibc-ld
STRIP=mipsel-uclibc-strip

DIR=$1
LIB_SO=$2
LIB_A=$3
LIB_SO_M=$4
SEARCHDIR=$5
INSTALLLIB=$6

MAP=${DIR}/.map
SYM=${DIR}/.sybmols
UNR=${DIR}/.unresolved
BINARIES=`find $SEARCHDIR -path $SEARCHDIR/lib -prune -o -type f -print | file -f - | grep ELF | cut -d':' -f1`

if [ ! -f ${DIR}/${LIB_SO} ] ; then
	echo "Cann't find ${DIR}/${LIB_SO}";
	exit 0;
fi

if [ ! -f ${DIR}/${LIB_A} ] ; then
	echo "Cann't find ${DIR}/${LIB_A}";
	exit 0;
fi

rm -f $MAP
rm -f $SYM
rm -f $UNR

$NM -o --defined-only --no-sort ${DIR}/${LIB_SO} | cut -d' ' -f3 > $MAP
$NM --dynamic -u --no-sort $BINARIES | sort -u > $UNR
for symbol in `cat $UNR` ; do 
	if grep -q "^$symbol" $MAP ; then echo "-u $symbol" >> $SYM ;
fi ; done 

if ls $SYM ; then 
	xargs -t $LD -shared -o ${DIR}/${LIB_SO_M} ${DIR}/${LIB_A} < $SYM ;
fi

if [ "a$INSTALLLIB" != "a" -a -f ${DIR}/${LIB_SO_M} ] ; then
	install ${DIR}/${LIB_SO_M} $INSTALLLIB
	$STRIP $INSTALLLIB
fi

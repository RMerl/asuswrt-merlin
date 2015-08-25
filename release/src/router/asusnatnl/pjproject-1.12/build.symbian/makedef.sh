#!/bin/sh

MMP=$1
if test "$MMP" == ""; then
	echo "Usage: makedef.sh FILE.MMP"
	echo "    or makedef.sh all"
	exit 1
fi

if test "$MMP" == "all"; then
	. $0 pjlib.mmp
	. $0 pjlib_util.mmp
	. $0 pjnath.mmp
	. $0 pjmedia.mmp
	. $0 pjsdp.mmp
	. $0 pjsip.mmp
	. $0 pjsip_simple.mmp
	. $0 pjsip_ua.mmp
	. $0 pjsua_lib.mmp
	. $0 symbian_audio.mmp
	. $0 null_audio.mmp
	exit 0
fi

if test -f $MMP; then
	true
else
	echo "Unable to open $MMP"
	exit 1
fi

TARGET=`grep -w '^TARGET' $MMP | awk '{print $2}' | awk -F '.' '{print $1}' | head -1`
DEFFILE="${TARGET}U.def"
SOURCES=`grep -w '^SOURCE' $MMP | awk '{print $2}' | tr '\\\\' '/'`
SOURCEPATH=`grep -w '^SOURCEPATH' $MMP | tr '\\\\' '/' | awk '{print $2}'`
INCPATH=`grep 'INCLUDE' $MMP | awk '{print $2}' | grep pj | tr '\\\\' '/'`
INCLUDE=""
for INC in $INCPATH; do
	INCLUDE=`echo $INCLUDE -I$INC`
done

#-- debug --
#echo TARGET=$TARGET
#echo SOURCES=$SOURCES
#echo SOURCEPATH=$SOURCEPATH
#echo INCLUDE=$INCLUDE
#-- end --

echo > tmpnames.def

echo "${TARGET}:"

for file in $SOURCES; do
	#SYMBOLS=`grep PJ_DEF ${SOURCEPATH}/$file | awk -F ')' '{print $2}' | awk -F '(' '{print $1}' | awk -F '=' '{print $1}' | tr -d '[:blank:]' | sort | uniq`
	SYMBOLS=`
		cpp -DPJ_SYMBIAN=1 -DPJ_DLL -DPJ_EXPORTING=1 $INCLUDE ${SOURCEPATH}/$file 2>&1 |
       		grep EXPORT_C | 
		sed 's/(/;/' | 
		sed 's/=/;/' | 
		awk -F ';' '{print $1}' | 
		awk '{print $NF}'`
	echo Processing ${SOURCEPATH}/$file..
	for SYM in $SYMBOLS; do
		echo $SYM >> tmpnames.def
	done
done

echo "Writing $DEFFILE"
echo EXPORTS > $DEFFILE
i=0
for SYM in `cat tmpnames.def | sort | uniq`; do
	echo "             $SYM"
	i=`expr $i + 1`
	printf "\\t%-40s @ $i NONAME\\n" $SYM >> $DEFFILE
done


echo
echo "Done. Total $i symbols exported in $DEFFILE."


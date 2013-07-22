#!/bin/sh

TEST_DIR=$TEST_FILE_SYSTEM_MOUNT_DIR
if test -z "$TEST_DIR";
then
	TEST_DIR="/mnt/test_file_system"
fi

FREESPACE=`../utils/free_space "$TEST_DIR"`

if test -z "$FREESPACE";
then
	echo "Failed to determine free space"
	exit 1
fi

if test -n "$1";
then
	DURATION="-d$1";
else
	DURATION="";
fi

FWRITE00=atoms/fwrite00
RNDWR=atoms/rndwrite00
PDFLUSH=atoms/pdfrun
FSIZE=$(( $FREESPACE/15 ));

../utils/fstest_monitor $DURATION \
"$FWRITE00 -z $FSIZE -n0 -p 300" \
"$FWRITE00 -z $FSIZE -n0 -u" \
"$FWRITE00 -z $FSIZE -n0 -u -c" \
"$FWRITE00 -z $FSIZE -n0 -s -o" \
"$RNDWR -z $FSIZE -n0 -e"

STATUS=$?

rm -rf ${TEST_DIR}/*

exit $STATUS

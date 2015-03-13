#!/bin/sh
COLL_INTL=`nvram get bwdpi_coll_intl`
CFG_POLL_INTL=43200
DC_MODE=`nvram get bwdpi_debug_path`

if [ -z $DC_MODE ]; then
	DC_CMD="data_colld"
	DC_PATH="/tmp/bwdpi/dc/"
	JFFS_TM="."
else
	if [ $DC_MODE == "1" ]; then
		echo "debug mode...."
		DC_CMD="/jffs/TM/data_colld"
		DC_PATH="/jffs/TM/dc/"
		JFFS_TM="/jffs/TM/"
	else
		DC_CMD="data_colld"
		DC_PATH="/tmp/bwdpi/dc/"
		JFFS_TM="."
	fi
fi


cmd="$1"
[ -z "$cmd" ] && cmd="start"

case "$cmd" in
start)
	echo "Start data collection daemon..."
	LD_LIBRARY_PATH=$JFFS_TM $DC_CMD -i $COLL_INTL -p $CFG_POLL_INTL -b -w $DC_PATH > /dev/null 2>&1
	;;
stop)
	echo "Stop data collection daemon..."
	killall -9 data_colld
	;;
esac

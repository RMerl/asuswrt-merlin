#!/bin/sh

COLL_INTL=`nvram get bwdpi_coll_intl`
CFG_POLL_INTL=43200
DC_PATH="/tmp/bwdpi/dc"

cmd="$1"
[ -z "$cmd" ] && cmd="start"

case "$cmd" in
start)
	echo "Start data collection daemon..."
	LD_LIBRARY_PATH=. data_colld -i $COLL_INTL -p $CFG_POLL_INTL -b -w $DC_PATH > /dev/null 2>&1
	;;
stop)
	echo "Stop data collection daemon..."
	killall -9 data_colld
	;;
esac

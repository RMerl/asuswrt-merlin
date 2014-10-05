#!/bin/sh

COLL_INTL=1800
CFG_POLL_INTL=43200
DC_PATH="/tmp/bwdpi/dc"

cmd="$1"
[ -z "$cmd" ] && cmd="start"

case "$cmd" in
start)
	echo "Start data collection daemon..."
	LD_LIBRARY_PATH=. data_colld -i $COLL_INTL -p $CFG_POLL_INTL -b -w $DC_PATH
	sleep 3
	service restart_firewall
	;;
stop)
	echo "Stop data collection daemon..."
	killall -9 data_colld
	;;
esac

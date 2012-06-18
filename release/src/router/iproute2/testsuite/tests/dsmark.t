#!/bin/bash
# vim: ft=sh

source lib/generic.sh

ts_qdisc_available "dsmark"
if [ $? -eq 0 ]; then
	ts_log "dsmark: Unsupported by $TC, skipping"
	exit 127
fi

ts_tc "dsmark" "dsmark root qdisc creation" \
	qdisc add dev $DEV root handle 10:0 \
	dsmark indices 64 default_index 1 set_tc_index

ts_tc "dsmark" "dsmark class 1 creation" \
	class change dev $DEV parent 10:0 classid 10:12 \
	dsmark mask 0xff value 2

ts_tc "dsmark" "dsmark class 2 creation" \
	class change dev $DEV parent 10:0 classid 10:13 \
	dsmark mask 0xfc value 4

ts_tc "dsmark" "dsmark dump qdisc" \
	qdisc list dev $DEV

ts_tc "dsmark" "dsmark dump class" \
	class list dev $DEV parent 10:0

ts_tc "dsmark" "generic qdisc tree deletion" \
	qdisc del dev $DEV root

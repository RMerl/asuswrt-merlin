#!/bin/bash
# vim: ft=sh

source lib/generic.sh

QDISCS="cbq htb dsmark"

if [ ! -d tests/cls ]; then
    ts_log "tests/cls folder does not exist"
    ts_skip
fi

for q in ${QDISCS}; do
	ts_log "Preparing classifier testbed with qdisc $q"

	for c in tests/cls/*.c; do

		case "$q" in
		cbq)
			ts_tc "cls-testbed" "cbq root qdisc creation" \
				qdisc add dev $DEV root handle 10:0 \
				cbq bandwidth 100Mbit avpkt 1400 mpu 64
			ts_tc "cls-testbed" "cbq root class creation" \
				class add dev $DEV parent 10:0  classid 10:12 \
				cbq bandwidth 100mbit rate 100mbit allot 1514 prio 3 \
				maxburst 1 avpkt  500 bounded
			;;
		htb)
			ts_qdisc_available "htb"
			if [ $? -eq 0 ]; then
				ts_log "cls-testbed: HTB is unsupported by $TC, skipping"
				continue;
			fi
			ts_tc "cls-testbed" "htb root qdisc creation" \
				qdisc add dev $DEV root handle 10:0 htb
			ts_tc "cls-testbed" "htb root class creation" \
				class add dev $DEV parent 10:0 classid 10:12 \
				htb rate 100Mbit quantum 1514
			;;
		dsmark)
			ts_qdisc_available "dsmark"
			if [ $? -eq 0 ]; then
				ts_log "cls-testbed: dsmark is unsupported by $TC, skipping"
				continue;
			fi
			ts_tc "cls-testbed" "dsmark root qdisc creation" \
				qdisc add dev $DEV root handle 20:0 \
				dsmark indices 64 default_index 1 set_tc_index
			ts_tc "cls-testbed" "dsmark class creation" \
				class change dev $DEV parent 20:0 classid 20:12 \
				dsmark mask 0xff value 2
			ts_tc "cls-testbed" "prio inner qdisc creation" \
				qdisc add dev $DEV parent 20:0 handle 10:0 prio
			;;
		*)
			ts_err "cls-testbed: no testbed configuration found for qdisc $q"
			continue
			;;
		esac

		ts_tc "cls-testbed" "tree listing" qdisc list dev eth0
		ts_tc "cls-testbed" "tree class listing" class list dev eth0
		ts_log "cls-testbed: starting classifier test $c"
		$c 

		case "$q" in
		*)
			ts_tc "cls-testbed" "generic qdisc tree deletion" \
				qdisc del dev $DEV root
			;;
		esac
	done
done

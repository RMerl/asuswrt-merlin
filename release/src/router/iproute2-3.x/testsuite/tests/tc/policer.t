#!/bin/sh
$TC qdisc del dev $DEV root >/dev/null 2>&1
$TC qdisc add dev $DEV root handle 10:0 cbq bandwidth 100Mbit avpkt 1400 mpu 64
$TC class add dev $DEV parent 10:0  classid 10:12   cbq bandwidth 100mbit rate 100mbit allot 1514 prio 3 maxburst 1 avpkt  500 bounded
$TC filter add dev $DEV parent 10:0 protocol ip prio 10 u32 match ip protocol 1 0xff police rate 2kbit buffer 10k drop flowid 10:12
$TC qdisc list dev $DEV
$TC filter list dev $DEV parent 10:0
$TC qdisc del dev $DEV root
$TC qdisc list dev $DEV
$TC qdisc add dev $DEV root handle 10:0 cbq bandwidth 100Mbit avpkt 1400 mpu 64
$TC class add dev $DEV parent 10:0  classid 10:12   cbq bandwidth 100mbit rate 100mbit allot 1514 prio 3 maxburst 1 avpkt  500 bounded
$TC filter add dev $DEV parent 10:0 protocol ip prio 10 u32 match ip protocol 1 0xff police rate 2kbit buffer 10k drop flowid 10:12
$TC qdisc del dev $DEV root

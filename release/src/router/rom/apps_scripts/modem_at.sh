#!/bin/sh
# $1: AT cmd, $2: waiting time.
# echo "This is a script to execute the AT command."


modem_type=`nvram get usb_modem_act_type`
act_node1="usb_modem_act_int"
act_node2="usb_modem_act_bulk"
modem_vid=`nvram get usb_modem_act_vid`

at_ret="/tmp/at_ret"


act_node=
#if [ "$modem_type" == "tty" -o "$modem_type" == "mbim" ]; then
#	if [ "$modem_type" == "tty" -a "$modem_vid" == "6610" ]; then # e.q. ZTE MF637U
#		act_node=$act_node1
#	else
#		act_node=$act_node2
#	fi
#else
	act_node=$act_node1
#fi

modem_act_node=`nvram get $act_node`
if [ "$modem_act_node" == "" ]; then
	find_modem_node.sh

	modem_act_node=`nvram get $act_node`
	if [ "$modem_act_node" == "" ]; then
		echo "Can't get $act_node!"
		exit 1
	fi
fi

if [ "$2" != "" ]; then
	waited_sec=$2
else
	waited_sec=1
fi

if [ "$1" == "" ]; then
	echo "Didn't input the AT command yet."
	exit 2
fi

chat -t $waited_sec -e '' "AT$1" OK >> /dev/$modem_act_node < /dev/$modem_act_node 2>$at_ret
ret=$?
cat $at_ret
exit $ret


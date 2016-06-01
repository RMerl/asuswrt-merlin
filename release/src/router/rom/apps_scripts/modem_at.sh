#!/bin/sh
# $1: AT cmd, $2: waiting time, $3: forcely use the bulk node.
# echo "This is a script to execute the AT command."


modem_type=`nvram get usb_modem_act_type`
act_node1="usb_modem_act_int"
act_node2="usb_modem_act_bulk"
modem_vid=`nvram get usb_modem_act_vid`

at_ret="/tmp/at_ret"

if [ -n "$3" -a "$3" == "bulk" ]; then
	act_node=$act_node2
else
	act_node=$act_node1
fi

modem_act_node=`nvram get $act_node`
if [ -n "$3" -a "$3" == "bulk" -a "$modem_act_node" == "" ]; then
	act_node=$act_node1
	modem_act_node=`nvram get $act_node`
fi

if [ "$modem_act_node" == "" ]; then
	/usr/sbin/find_modem_node.sh

	modem_act_node=`nvram get $act_node`
	if [ "$modem_act_node" == "" ]; then
		echo "Can't get $act_node!"
		exit 1
	fi
fi
#echo "modem_act_node=$act_node"

if [ -n "$2" ]; then
	waited_sec=$2
else
	waited_sec=1
fi

if [ -z "$1" ]; then
	echo "Didn't input the AT command yet."
	exit 2
fi
chat -t $waited_sec -e '' "AT$1" OK >> /dev/$modem_act_node < /dev/$modem_act_node 2>$at_ret
ret=$?
cat $at_ret
exit $ret


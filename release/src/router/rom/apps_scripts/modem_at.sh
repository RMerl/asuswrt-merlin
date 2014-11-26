#!/bin/sh
# $1: AT cmd, $2: waiting time.
# echo "This is a script to execute the AT command."


modem_type=`nvram get usb_modem_act_type`
modem_act_node1=`nvram get usb_modem_act_int`
modem_act_node2=`nvram get usb_modem_act_bulk`
modem_vid=`nvram get usb_modem_act_vid`

at_ret="/tmp/at_ret"


modem_act_node=
if [ "$modem_type" == "tty" -o "$modem_type" == "mbim" ]; then
	if [ "$modem_type" == "tty" -a "$modem_vid" == "6610" ]; then # e.q. ZTE MF637U
		modem_act_node=$modem_act_node1
	else
		modem_act_node=$modem_act_node2
	fi
else
	modem_act_node=$modem_act_node1
fi

if [ "$2" != "" ]; then
	waited_sec=$2
else
	waited_sec=1
fi

if [ "$modem_act_node" == "" ]; then
	echo "Can't get the active serial node."
	exit 1
fi

if [ "$1" == "" ]; then
	echo "Didn't input the AT command yet."
	exit 2
fi

chat -t $waited_sec -e '' "AT$1" OK >> /dev/$modem_act_node < /dev/$modem_act_node 2>$at_ret
ret=$?
cat $at_ret
exit $ret


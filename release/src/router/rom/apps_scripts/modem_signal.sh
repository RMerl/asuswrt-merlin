#!/bin/sh
echo "This is a script to get the modem signal out."


modem_enable=`nvram get modem_enable`
modem_act_node1=`nvram get usb_modem_act_int`
modem_act_node2=`nvram get usb_modem_act_bulk`
modem_type=`nvram get usb_modem_act_type`


modem_act_node=
if [ "$modem_type" == "tty" ]; then
	modem_act_node=$modem_act_node2
else
	modem_act_node=$modem_act_node1
fi

if [ "$modem_act_node" == "" ]; then
	echo "Can't get the active serial node."
	nvram set usb_modem_act_signal=
	exit 1
fi

chat -t 1 -e '' 'AT+CSQ' OK >> /dev/$modem_act_node < /dev/$modem_act_node 2>/tmp/at_ret
ret=`grep "+CSQ: " /tmp/at_ret |awk '{FS=": "; print $2}' |awk '{FS=",99"; print $1}'`

if [ "$ret" == "" ]; then
	echo "Fail to get the signal from $modem_act_node."
	nvram set usb_modem_act_signal=
	exit 2
fi

signal=
if [ $ret -eq 99 ]; then
	# not known or not detectable.
	signal=-1
elif [ $ret -le 1 ]; then
	# almost no signal.
	signal=0
elif [ $ret -le 9 ]; then
	# Marginal.
	signal=1
elif [ $ret -le 14 ]; then
	# OK.
	signal=2
elif [ $ret -le 19 ]; then
	# Good.
	signal=3
elif [ $ret -le 30 ]; then
	# Excellent.
	signal=4
elif [ $ret -eq 31 ]; then
	# Full.
	signal=5
else
	echo "Can't identify the signal strength."
	nvram set usb_modem_act_signal=
	exit 3
fi

echo "$signal"
nvram set usb_modem_act_signal=$signal

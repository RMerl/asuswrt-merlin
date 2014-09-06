#!/bin/sh
# $1: type.
echo "This is a script to get the modem status."


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
	nvram set usb_modem_act_sim=0
	exit 1
fi

if [ "$1" == "sim" ]; then
	# check the SIM status.
	chat -t 1 -e '' 'AT+CPIN?' OK >> /dev/$modem_act_node < /dev/$modem_act_node 2>/tmp/at_ret
	sim_inserted1=`grep "READY" /tmp/at_ret`
	sim_inserted2=`grep "SIM PIN" /tmp/at_ret`
	sim_inserted3=`grep "SIM PUK" /tmp/at_ret`
	if [ -z "$sim_inserted1" ] && [ -z "$sim_inserted2" ] && [ -z "$sim_inserted3" ]; then # No SIM.
		echo "No or incorrect SIM card."
		nvram set usb_modem_act_sim=0
	else
		echo "Got SIM."
		nvram set usb_modem_act_sim=1
	fi
elif [ "$1" == "signal" ]; then
	nvram set usb_modem_act_signal=

	chat -t 1 -e '' 'AT+CSQ' OK >> /dev/$modem_act_node < /dev/$modem_act_node 2>/tmp/at_ret
	ret=`grep "+CSQ: " /tmp/at_ret |awk '{FS=": "; print $2}' |awk '{FS=",99"; print $1}'`
	if [ "$ret" == "" ]; then
		echo "Fail to get the signal from $modem_act_node."
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
		echo "Can't identify the signal strength: $ret."
		exit 3
	fi

	echo "$signal"
	nvram set usb_modem_act_signal=$signal
elif [ "$1" == "operation" ]; then
	nvram set usb_modem_act_operation=

	chat -t 1 -e '' 'AT$CBEARER' OK >> /dev/$modem_act_node < /dev/$modem_act_node 2>/tmp/at_ret
	ret=`grep "$CBEARER:" /tmp/at_ret |awk '{FS=":"; print $2}'`
	if [ "$ret" == "" ]; then
		echo "Fail to get the operation type from $modem_act_node."
		exit 4
	fi

	operation=
	if [ "$ret" == "0x01" ]; then
		operation=GPRS
	elif [ "$ret" == "0x02" ]; then
		operation=EDGE
	elif [ "$ret" == "0x03" ]; then
		operation=HSDPA
	elif [ "$ret" == "0x04" ]; then
		operation=HSUPA
	elif [ "$ret" == "0x05" ]; then
		operation=WCDMA
	elif [ "$ret" == "0x06" ]; then
		operation=CDMA
	elif [ "$ret" == "0x07" ]; then
		operation="EV-DO REV 0"
	elif [ "$ret" == "0x08" ]; then
		operation="EV-DO REV A"
	elif [ "$ret" == "0x09" ]; then
		operation=GSM
	elif [ "$ret" == "0x0A" ]; then
		operation="EV-DO REV B"
	elif [ "$ret" == "0x0B" ]; then
		operation=LTE
	elif [ "$ret" == "0x0C" ]; then
		operation="HSDPA+"
	elif [ "$ret" == "0x0D" ]; then
		operation="DC-HSDPA+"
	else
		echo "Can't identify the operation type: $ret."
		exit 5
	fi

	echo "$operation"
	nvram set usb_modem_act_operation="$operation"
fi


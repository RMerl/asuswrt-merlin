#!/bin/sh
# echo "This is a script to enable the modem."


modem_type=`nvram get usb_modem_act_type`
modem_vid=`nvram get usb_modem_act_vid`
modem_pid=`nvram get usb_modem_act_pid`
modem_dev=`nvram get usb_modem_act_dev`

usb_gobi2=`nvram get usb_gobi2`


# $1: ifname.
_get_qcqmi_by_usbnet(){
	rp1=`readlink -f /sys/class/net/$1/device 2>/dev/null`
	if [ "$rp1" == "" ]; then
		echo ""
		return
	fi

	rp2=
	i=0
	while [ $i -lt 10 ]; do
		rp2=`readlink -f /sys/class/GobiQMI/qcqmi$i/device 2>/dev/null`
		if [ "$rp2" == "" ]; then
			i=$((i+1))
			continue
		fi

		if [ "$rp1" == "$rp2" ]; then
			echo "qcqmi$i"
			return
		fi

		i=$((i+1))
	done

	echo ""
}


if [ "$modem_type" == "gobi" ]; then
	if [ "$usb_gobi2" == "1" ]; then
		echo "Gobi2: Pause the autoconnect."
		at_ret=`$at_lock /usr/sbin/modem_at.sh "+CAUTOCONNECT=0" |grep "OK" 2>/dev/null`
		if [ "$at_ret" != "OK" ]; then
			echo "Gobi2: Fail to stop the autoconnect."
			exit 0
		fi
		at_ret=`$at_lock /usr/sbin/modem_at.sh "+CWWAN=0" |grep "OK" 2>/dev/null`
		if [ "$at_ret" != "OK" ]; then
			echo "Gobi2: Fail to stop the connection."
			exit 0
		fi
	else
		qcqmi=`_get_qcqmi_by_usbnet $modem_dev`
		echo "Got qcqmi: $qcqmi."

		cmd_pipe="/tmp/pipe"

		gobi_pid=`pidof gobi`
		if [ "$gobi_pid" == "" ]; then
			gobi d &
			sleep 1
		fi

		# connect to GobiNet.
		echo -n "1,$qcqmi" >> $cmd_pipe
		sleep 1

		# WDS stop the data session
		echo -n "4" >> $cmd_pipe
		sleep 1

		echo -n "12" >> $cmd_pipe
		sleep 1

		echo -n "99" >> $cmd_pipe
		sleep 1
	fi

	echo "Gobi: Successfull to stop network."
fi


#!/bin/sh
# echo "This is a script to enable the modem."


modem_type=`nvram get usb_modem_act_type`
modem_vid=`nvram get usb_modem_act_vid`
modem_pid=`nvram get usb_modem_act_pid`
modem_dev=`nvram get usb_modem_act_dev`
modem_reg_time=`nvram get modem_reg_time`
wandog_interval=`nvram get wandog_interval`
atcmd=`nvram get modem_atcmd`

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
		at_ret=`/usr/sbin/modem_at.sh "+CAUTOCONNECT=0" 2>&1`
		ret=`echo -n $at_ret |grep "OK" 2>/dev/null`
		if [ -z "$ret" ]; then
			echo "Gobi2: Fail to stop the autoconnect."
			exit 0
		fi
		at_ret=`/usr/sbin/modem_at.sh "+CWWAN=0" 2>&1`
		ret=`echo -n $at_ret |grep "OK" 2>/dev/null`
		if [ -z "$ret" ]; then
			echo "Gobi2: Fail to stop the connection."
			exit 0
		fi
	else
		qcqmi=`_get_qcqmi_by_usbnet $modem_dev`
		echo "Got qcqmi: $qcqmi."

		gobi_api $qcqmi SetEnhancedAutoconnect 2 1

		wait_time1=`expr $wandog_interval + $wandog_interval`
		wait_time=`expr $wait_time1 + $modem_reg_time`
		nvram set freeze_duck=$wait_time
		/usr/sbin/modem_at.sh '+COPS=2' "$modem_reg_time"
		if [ -z "$atcmd" ] || [ "$atcmd" != "1" ]; then
			/usr/sbin/modem_at.sh '' # clean the output of +COPS=2.
		fi
	fi
fi

echo "$modem_type: Successfull to stop network."

#!/bin/sh
# $1: ifname.
echo "This is a script to enable the modem type out."


modem_enable=`nvram get modem_enable`
modem_act_node=`nvram get usb_modem_act_int`
modem_type=`nvram get usb_modem_act_type`
modem_vid=`nvram get usb_modem_act_vid`
modem_pid=`nvram get usb_modem_act_pid`
modem_pin=`nvram get modem_pincode`
modem_apn=`nvram get modem_apn`

# $1: ifname.
_get_wdm_by_usbnet(){
	rp1=`readlink -f /sys/class/net/$1/device 2>/dev/null`
	if [ "$rp1" == "" ]; then
		echo ""
		return
	fi

	rp2=
	i=0
	while [ $i -lt 10 ]; do
		rp2=`readlink -f /sys/class/usb/cdc-wdm$i/device 2>/dev/null`
		if [ "$rp2" == "" ]; then
			i=$((i+1))
			continue
		fi

		if [ "$rp1" == "$rp2" ]; then
			echo "/dev/cdc-wdm$i"
			return
		fi

		i=$((i+1))
	done

	echo ""
}


if [ "$modem_act_node" == "" ]; then
	find_modem_node.sh

	modem_act_node=`nvram get usb_modem_act_int`
	if [ "$modem_act_node" == "" ]; then
		echo "Can't get usb_modem_act_int!"
		exit 2
	fi
fi

# input PIN if need.
nvram set g3state_pin=0
chat -t 1 -e '' 'AT+CPIN?' OK >> /dev/$modem_act_node < /dev/$modem_act_node 2>/tmp/at_ret
pin_ret=`grep "+CPIN: " /tmp/at_ret |awk '{FS=": "; print $2}'`
#if [ "$pin_ret" != "" ]; then
if [ "$modem_enable" == "1" ] && [ "$pin_ret" != "READY" ]; then # Temp for WCDMA/LTE.
	nvram set g3state_pin=1
	if [ "$pin_ret" == "SIM PIN" ] && [ "$modem_pin" != "" ]; then
		chat -t 1 -e '' 'AT+CPIN='$modem_pin OK >> /dev/$modem_act_node < /dev/$modem_act_node 2>/tmp/at_ret
		pin_ret=`grep "+CPIN: " /tmp/at_ret`

		chat -t 1 -e '' 'AT+CPIN?' OK >> /dev/$modem_act_node < /dev/$modem_act_node 2>/tmp/at_ret
		pin_ret=`grep "+CPIN: " /tmp/at_ret |awk '{FS=": "; print $2}'`
	fi

	if [ "$pin_ret" == "READY" ]; then
		echo "SIM is ready."
	else
		echo "Incorrect SIM card or can't input the correct PIN code."
		exit 1
	fi
fi

if [ "$modem_vid" == "8193" ];then
	# just for D-Link DWM-156 A7.
	#chat -t 1 -e '' 'AT+BMHPLMN' OK >> /dev/$modem_act_node < /dev/$modem_act_node 2>/tmp/at_ret
	#ret=`grep "+BMHPLMN: " /tmp/at_ret`
	#if [ "$ret" != "" ]; then
	#	plmn=`echo $ret | awk '{print $2}'`
	#	echo "IMSI: $plmn."
	#fi

	chat -t 1 -e '' 'AT+CFUN=1' OK >> /dev/$modem_act_node < /dev/$modem_act_node 2>/tmp/at_ret
	ret=`grep OK /tmp/at_ret`
	if [ "$ret" != "OK" ]; then
		echo "Fail to set CFUN=1."
		exit 1
	fi

	tries=1
	ret=""
	while [ $tries -le 30 ] && [ "$ret" == "" ]; do
		echo "wait for network registered...$tries"
		sleep 1

		chat -t 1 -e '' 'AT+CGATT?' OK >> /dev/$modem_act_node < /dev/$modem_act_node 2>/tmp/at_ret
		ret=`grep "+CGATT: 1" /tmp/at_ret`
		tries=$((tries + 1))
	done

	if [ "$ret" == "" ]; then
		echo "fail to register network, please check."
		exit 1
	fi

	echo "successfull to register network."
elif [ "$modem_type" == "qmi" ]; then
	wdm=`_get_wdm_by_usbnet $1`
	modem_user=`nvram get modem_user`
	modem_pass=`nvram get modem_pass`
	if [ "$modem_user" != "" ]; then
		modem_user="--username $modem_user "
	fi
	if [ "$modem_pass" != "" ]; then
		modem_pass="--password $modem_pass "
	fi

	echo "QMI: try if the network is registered..."
	uqmi -d $wdm --keep-client-id wds --start-network $modem_apn $modem_user $modem_pass >/tmp/at_ret
	ret=`cat /tmp/at_ret |grep "handle="`
	if [ "$ret" != "" ]; then
		echo "successfull to register network."
		exit 0
	elif [ "$modem_vid" == "4817" ] && [ "$modem_pid" == "5132" ]; then
		echo "Restarting the MT and set it as online mode..."
		nvram set usb_modem_reset_huawei=1
		chat -t 1 -e '' 'AT+CFUN=1,1' OK >> /dev/$modem_act_node < /dev/$modem_act_node 2>/tmp/at_ret
		tries=1
		ret=""
		while [ $tries -le 30 ] && [ "$ret" == "" ]; do
			echo "wait the modem to wake up...$tries"
			sleep 1

			chat -t 1 -e '' 'AT+CGATT?' OK >> /dev/$modem_act_node < /dev/$modem_act_node 2>/tmp/at_ret
			ret=`grep "+CGATT: 1" /tmp/at_ret`
			tries=$((tries + 1))
		done

		if [ "$ret" == "" ]; then
			echo "fail to reset the modem, please check."
			exit 1
		fi

		echo "successfull to reset the modem."
		nvram unset usb_modem_reset_huawei
	fi

	tries=1
	ret=""
	while [ $tries -le 15 ]; do
		echo "QMI: wait for network registered...$tries"
		sleep 1

		uqmi -d $wdm --keep-client-id wds --start-network $modem_apn $modem_user $modem_pass>/tmp/at_ret
		ret=`cat /tmp/at_ret |grep "handle="`
		if [ "$ret" != "" ]; then
			break
		fi

		tries=$((tries + 1))
	done

	if [ "$ret" == "" ]; then
		echo "fail to register network, please check."
		exit 1
	fi

	echo "successfull to register network."
fi


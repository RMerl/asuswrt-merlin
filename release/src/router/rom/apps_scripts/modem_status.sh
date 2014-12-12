#!/bin/sh
# $1: type.
# echo "This is a script to get the modem status."


modem_type=`nvram get usb_modem_act_type`
act_node1="usb_modem_act_int"
act_node2="usb_modem_act_bulk"
modem_vid=`nvram get usb_modem_act_vid`
modem_pid=`nvram get usb_modem_act_pid`
modem_dev=`nvram get usb_modem_act_dev`
modem_roaming_scantime=`nvram get modem_roaming_scantime`
modem_roaming_scanlist=`nvram get modem_roaming_scanlist`

at_lock="flock -x /tmp/at_cmd_lock"

jffs_dir="/jffs"


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

if [ "$1" == "bytes" -o "$1" == "bytes+" ]; then
	if [ "$1" == "bytes+" ]; then
		modem_bytes_plus
	fi

	if [ "$modem_dev" == "" ]; then
		echo "Can't get the active network device of USB."
		exit 2
	fi

	ret=`ifconfig "$modem_dev" |grep 'bytes:' 2>/dev/null`
	rx_new=`echo "$ret" |awk '{FS="RX bytes:"; print $2}' |awk '{FS=" "; print $1}' 2>/dev/null`
	tx_new=`echo "$ret" |awk '{FS="TX bytes:"; print $2}' |awk '{FS=" "; print $1}' 2>/dev/null`

	echo $rx_new > ${jffs_dir}/modem_bytes_rx
	echo $tx_new > ${jffs_dir}/modem_bytes_tx

	nvram set modem_bytes_rx=$rx_new
	nvram set modem_bytes_tx=$tx_new
elif [ "$1" == "bytes-" ]; then
	echo 0 > ${jffs_dir}/modem_bytes_rx
	echo 0 > ${jffs_dir}/modem_bytes_tx
	echo 0 > ${jffs_dir}/modem_bytes_rx_old
	echo 0 > ${jffs_dir}/modem_bytes_tx_old

	nvram set modem_bytes_rx=0
	nvram set modem_bytes_tx=0
	nvram set modem_bytes_rx_old=0
	nvram set modem_bytes_tx_old=0
elif [ "$1" == "sim" ]; then
	# check the SIM status.
	at_ret=`$at_lock modem_at.sh '+CPIN?' 2>/dev/null`
	sim_inserted1=`echo "$at_ret" |grep "READY" 2>/dev/null`
	sim_inserted2=`echo "$at_ret" |grep "SIM" |awk '{FS=": "; print $2}' 2>/dev/null`
	sim_inserted3=`echo "$at_ret" |grep "+CME ERROR: " |awk '{FS=": "; print $2}' 2>/dev/null`
	sim_inserted4=`echo "$sim_inserted2" |cut -c 1-3`
	if [ -n "$sim_inserted1" ]; then
		echo "Got SIM."
		act_sim=1
	elif [ "$sim_inserted2" == "SIM PIN" ]; then
		echo "Need PIN."
		act_sim=2
	elif [ "$sim_inserted2" == "SIM PUK" ]; then
		echo "Need PUK."
		act_sim=3
	elif [ "$sim_inserted2" == "SIM PIN2" ]; then
		echo "Need PIN2."
		act_sim=4
	elif [ "$sim_inserted2" == "SIM PUK2" ]; then
		echo "Need PUK2."
		act_sim=5
	elif [ "$sim_inserted4" == "PH-" ]; then
		echo "Waiting..."
		act_sim=6
	elif [ "$sim_inserted3" != "" ]; then
		if [ "$sim_inserted3" == "SIM not inserted" ]; then
			echo "SIM not inserted."
			act_sim=-1
		else
			echo "CME ERROR: $sim_inserted3"
			act_sim=-2
		fi
	else
		echo "No or unknown response."
		act_sim=-10
	fi

	act_sim_orig=`nvram get usb_modem_act_sim`
	if [ "$act_sim_orig" != "$act_sim" ]; then
		nvram set usb_modem_act_sim=$act_sim
	fi
elif [ "$1" == "signal" ]; then
	at_ret=`$at_lock modem_at.sh '+CSQ' 2>/dev/null`
	ret=`echo "$at_ret" |grep "+CSQ: " |awk '{FS=": "; print $2}' |awk '{FS=",99"; print $1}' 2>/dev/null`
	if [ "$ret" == "" ]; then
		echo "Fail to get the signal from $modem_act_node."
		exit 3
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
		exit 4
	fi

	echo "$signal"
	nvram set usb_modem_act_signal=$signal
elif [ "$1" == "operation" ]; then
	if [ "$modem_vid" == "1478" -a "$modem_pid" == "36902" ]; then
		at_ret=`$at_lock modem_at.sh '$CBEARER' 2>/dev/null`
		ret=`echo "$at_ret" |grep '$CBEARER:' |awk '{FS=":"; print $2}' 2>/dev/null`
		if [ "$ret" == "" ]; then
			echo "Fail to get the operation type from $modem_act_node."
			exit 5
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
		elif [ "$ret" == "0x0a" -o "$ret" == "0x0A" ]; then
			operation="EV-DO REV B"
		elif [ "$ret" == "0x0b" -o "$ret" == "0x0B" ]; then
			operation=LTE
		elif [ "$ret" == "0x0c" -o "$ret" == "0x0C" ]; then
			operation="HSDPA+"
		elif [ "$ret" == "0x0d" -o "$ret" == "0x0D" ]; then
			operation="DC-HSDPA+"
		else
			echo "Can't identify the operation type: $ret."
			exit 6
		fi

		echo "$operation"
		nvram set usb_modem_act_operation="$operation"
	fi
elif [ "$1" == "setmode" ]; then
	if [ "$modem_vid" == "1478" -a "$modem_pid" == "36902" ]; then
		mode=
		if [ "$2" == "0" ]; then	# Auto
			mode=10
		elif [ "$2" == "43" ]; then	# 4G/3G
			mode=17
		elif [ "$2" == "4" ]; then	# 4G only
			mode=11
		elif [ "$2" == "3" ]; then	# 3G only
			mode=2
		elif [ "$2" == "2" ]; then	# 2G only
			mode=1
		else
			echo "Can't identify the mode type: $2."
			exit 7
		fi

		at_ret=`$at_lock modem_at.sh '+CSETPREFNET='$mode 2>/dev/null`
		ret=`echo "$at_ret" |grep '+CSETPREFNET=' |awk '{FS="="; print $2}' 2>/dev/null`
		if [ "$ret" == "" ]; then
			echo "Fail to set the modem mode from $modem_act_node."
			exit 8
		fi

		echo "Set the mode be $2($ret)."
	fi
elif [ "$1" == "getmode" ]; then
	if [ "$modem_vid" == "1478" -a "$modem_pid" == "36902" ]; then
		mode=

		at_ret=`$at_lock modem_at.sh '+CGETPREFNET' 2>/dev/null`
		ret=`echo "$at_ret" |grep '+CGETPREFNET:' |awk '{FS=":"; print $2}' 2>/dev/null`
		if [ "$ret" == "" ]; then
			echo "Fail to get the modem mode from $modem_act_node."
			exit 9
		elif [ "$ret" == "10" ]; then	# Auto
			mode=0
		elif [ "$ret" == "17" ]; then	# 4G/3G
			mode=43
		elif [ "$ret" == "11" ]; then	# 4G only
			mode=4
		elif [ "$ret" == "2" ]; then	# 3G only
			mode=3
		elif [ "$ret" == "1" ]; then	# 2G only
			mode=2
		else
			echo "Can't identify the mode type: $ret."
			exit 10
		fi

		echo "Get the mode be $mode."
	fi
elif [ "$1" == "imsi" ]; then
	at_ret=`$at_lock modem_at.sh '+CIMI' 2>/dev/null`
	ret=`echo "$at_ret" |grep "^[0-9][0-9]*$" 2>/dev/null`
	if [ "$ret" == "" ]; then
		echo "Fail to get the IMEI from $modem_act_node."
		exit 11
	fi

	echo "Got IMSI."
	nvram set usb_modem_act_imsi=$ret
elif [ "$1" == "imei" ]; then
	at_ret=`$at_lock modem_at.sh '+CGSN' 2>/dev/null`
	ret=`echo "$at_ret" |grep "^[0-9][0-9]*$" 2>/dev/null`
	if [ "$ret" == "" ]; then
		echo "Fail to get the IMEI from $modem_act_node."
		exit 12
	fi

	echo "Got IMEI."
	nvram set usb_modem_act_imei=$ret
elif [ "$1" == "iccid" ]; then
	if [ "$modem_vid" == "1478" -a "$modem_pid" == "36902" ]; then
		at_ret=`$at_lock modem_at.sh '+ICCID' 2>/dev/null`
		ret=`echo "$at_ret" |grep "ICCID: " |awk '{FS="ICCID: "; print $2}' 2>/dev/null`
		if [ "$ret" == "" ]; then
			echo "Fail to get the ICCID from $modem_act_node."
			exit 13
		fi

		echo "Got ICCID."
		nvram set usb_modem_act_iccid=$ret
	fi
elif [ "$1" == "rate" ]; then
	if [ "$modem_vid" == "1478" -a "$modem_pid" == "36902" ]; then
		qcqmi=`_get_qcqmi_by_usbnet $modem_dev 2>/dev/null`
		at_ret=`gobi_api $qcqmi rate |grep "Max Tx" 2>/dev/null`
		max_tx=`echo "$at_ret" |awk '{FS=","; print $1}' |awk '{FS=" "; print $3}' 2>/dev/null`
		max_rx=`echo "$at_ret" |awk '{FS=","; print $2}' |awk '{FS=" "; print $2}' |awk '{FS="."; print $1}' 2>/dev/null`
		if [ "$max_tx" == "" -o "$max_rx" == "" ]; then
			echo "Fail to get the IMEI from $modem_act_node."
			exit 14
		fi

		echo "Got rate."
		nvram set usb_modem_act_tx=$max_tx
		nvram set usb_modem_act_rx=$max_rx
	fi
elif [ "$1" == "hwver" ]; then
	if [ "$modem_vid" == "1478" -a "$modem_pid" == "36902" ]; then
		at_ret=`$at_lock modem_at.sh '$HWVER' 2>/dev/null`
		ret=`echo "$at_ret" |grep "^[0-9][0-9]*$" 2>/dev/null`
		if [ "$ret" == "" ]; then
			nvram set usb_modem_act_imei=
			echo "Fail to get the hardware version from $modem_act_node."
			exit 15
		fi

		echo "Got HWVER."
		nvram set usb_modem_act_hwver=$ret
	fi
elif [ "$1" == "band" ]; then
	if [ "$modem_vid" == "1478" -a "$modem_pid" == "36902" ]; then
		at_ret=`$at_lock modem_at.sh '$CRFI' 2>/dev/null`
		ret=`echo "$at_ret" |grep '$CRFI:' |awk '{FS=":"; print $2}' 2>/dev/null`
		if [ "$ret" == "" ]; then
			echo "Fail to get the current band from $modem_act_node."
			exit 16
		fi

		echo "Got Band: $ret."
		nvram set usb_modem_act_band="$ret"
	fi
elif [ "$1" == "scan" ]; then
	nvram set usb_modem_act_scanning=2
	at_ret=`$at_lock modem_at.sh '+COPS=2' |grep "OK" 2>/dev/null`

	at_ret=`$at_lock modem_at.sh '+COPS=?' $modem_roaming_scantime 2>/dev/null`
	ret=`echo "$at_ret" |grep '+COPS: ' |awk '{FS=": "; print $2}' |awk '{FS=",,"; print $1}' 2>/dev/null`
	nvram set usb_modem_act_scanning=1
	if [ "$ret" == "" ]; then
		echo "Fail to scan the stations."
		exit 17
	fi

	num=`echo "$ret" |awk '{FS=")"; print NF}' 2>/dev/null`
	if [ "$num" == "" ]; then
		echo "Fail to count the stations."
		exit 18
	fi

	list="["
	filter=""
	i=1
	while [ $i -lt $num ]; do
		str=`echo "$ret" |awk '{FS=")"; print $'$i'}' |awk '{FS="("; print $2}' 2>/dev/null`

		sta=`echo "$str" |awk '{FS=","; print $2}' 2>/dev/null`
		sta_code=`echo "$str" |awk '{FS=","; print $4}' 2>/dev/null`

		ret_filter=`echo $filter |grep ",$sta," 2>/dev/null`
		if [ "$ret_filter" == "" ]; then
			if [ "$list" != "[" ]; then
				list=$list",[$sta, $sta_code]"
			else
				list=$list"[$sta, $sta_code]"
			fi
			filter=$filter","$sta","
		fi

		i=$((i+1))
	done
	list=$list"]"
	echo -n "$list" > $modem_roaming_scanlist
	nvram set usb_modem_act_scanning=0
	echo "Got the list."
elif [ "$1" == "station" ]; then
	$at_lock modem_at.sh "+COPS=1,0,\"$2\"" 1,2>/dev/null
	if [ $? -ne 0 ]; then
		echo "Fail to set the station: $2."
		exit 19
	fi

	echo "Set the station."
elif [ "$1" == "simauth" ]; then
	if [ "$modem_vid" == "1478" -a "$modem_pid" == "36902" ]; then
		nvram set usb_modem_act_auth=
		nvram set usb_modem_act_auth_pin=
		nvram set usb_modem_act_auth_puk=
		at_ret=`$at_lock modem_at.sh '+CPINR' |grep "+CPINR:" |awk '{FS=":"; print $2}' 2>/dev/null`
		if [ "$at_ret" == "" ]; then
			echo "Fail to get the SIM status."
			exit 20
		fi

		ret=`echo "$at_ret" |awk '{FS=","; print $3}' 2>/dev/null`
		if [ "$ret" == "" ]; then
			echo "Fail to get the SIM auth state."
			exit 21
		fi
		nvram set usb_modem_act_auth=$ret
		if [ "$ret" == "1" ]; then
			echo "SIM auth state is ENABLED_NOT_VERIFIED."
		elif [ "$ret" == "2" ]; then
			echo "SIM auth state is ENABLED_VERIFIED."
		elif [ "$ret" == "3" ]; then
			echo "SIM auth state is DISABLED."
		elif [ "$ret" == "4" ]; then
			echo "SIM auth state is BLOCKED."
		elif [ "$ret" == "5" ]; then
			echo "SIM auth state is PERMANENTLY_BLOCKED."
		else
			echo "SIM auth state is UNKNOWN."
		fi

		ret=`echo "$at_ret" |awk '{FS=","; print $4}' 2>/dev/null`
		if [ "$ret" == "" ]; then
			echo "Fail to get the PIN retry."
			exit 22
		fi
		nvram set usb_modem_act_auth_pin=$ret
		echo "SIM PIN retry is $ret."

		ret=`echo "$at_ret" |awk '{FS=","; print $5}' 2>/dev/null`
		if [ "$ret" == "" ]; then
			echo "Fail to get the PUK retry."
			exit 23
		fi
		nvram set usb_modem_act_auth_puk=$ret

		echo "SIM PUK retry is $ret."
	fi
elif [ "$1" == "simpin" ]; then
	if [ "$2" == "" ]; then
		nvram set g3state_pin=2

		echo "Need to input the PIN code."
		exit 24
	fi

	nvram set g3state_pin=1
	at_ret=`$at_lock modem_at.sh '+CPIN='$2 |grep "+CPIN=$2" 2>/dev/null`
	if [ "$at_ret" == "" ]; then
		nvram set g3err_pin=1

		echo "Fail to unlock the SIM: $2."
		exit 25
	fi

	echo "done."
elif [ "$1" == "simpuk" ]; then
	if [ "$2" == "" ]; then
		echo "Need to input the PUK code."
		exit 26
	elif [ "$3" == "" ]; then
		echo "Need to input the new PIN code."
		exit 27
	fi

	at_ret=`$at_lock modem_at.sh '+CPIN='$2','$3 |grep "+CPIN=$2" 2>/dev/null`
	if [ "$at_ret" == "" ]; then
		echo "Fail to unlock the SIM PIN: $2."
		exit 28
	fi

	echo "done."
elif [ "$1" == "lockpin" ]; then
	simauth=`nvram get usb_modem_act_auth`
	if [ "$simauth" == "1" ]; then
		echo "SIM need to input the PIN code first."
		exit 29
	elif [ "$simauth" == "4" -o "$simauth" == "5" ]; then # lock
		echo "SIM had been blocked."
		exit 30
	elif [ "$simauth" == "0" ]; then # lock
		echo "Can't get the SIM auth state."
		exit 31
	fi

	if [ "$2" == "" ]; then
		echo "decide to lock/unlock PIN."
		exit 32
	fi

	if [ "$3" == "" ]; then
		echo "need the PIN code."
		exit 33
	fi

	if [ "$2" == "1" ]; then # lock
		if [ "$simauth" == "2" ]; then
			echo "had locked."
			exit 0
		fi

		at_ret=`$at_lock modem_at.sh '+CLCK="SC",1,"'$3'"' |grep "OK" 2>/dev/null`
		if [ "$at_ret" == "2" ]; then
			echo "fail to lock PIN."
			exit 34
		fi
	else
		if [ "$simauth" == "3" ]; then
			echo "had unlocked."
			exit 0
		fi

		at_ret=`$at_lock modem_at.sh '+CLCK="SC",0,"'$3'"' |grep "OK" 2>/dev/null`
		if [ "$at_ret" == "2" ]; then
			echo "fail to unlock PIN."
			exit 35
		fi
	fi

	echo "done."
elif [ "$1" == "pwdpin" ]; then
	if [ "$2" == "" ]; then
		echo "Need to input the original PIN code."
		exit 36
	elif [ "$3" == "" ]; then
		echo "Need to input the new PIN code."
		exit 37
	fi

	at_ret=`$at_lock modem_at.sh '+CPWD="SC",'$2','$3 |grep "OK" 2>/dev/null`
	if [ "$at_ret" == "" ]; then
		echo "Fail to change the PIN."
		exit 38
	fi

	echo "done."
fi


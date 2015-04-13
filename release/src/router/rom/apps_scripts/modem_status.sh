#!/bin/sh
# $1: type.
# echo "This is a script to get the modem status."


modem_enable=`nvram get modem_enable`
modem_type=`nvram get usb_modem_act_type`
act_node1="usb_modem_act_int"
act_node2="usb_modem_act_bulk"
modem_vid=`nvram get usb_modem_act_vid`
modem_pid=`nvram get usb_modem_act_pid`
modem_dev=`nvram get usb_modem_act_dev`
modem_roaming_scantime=`nvram get modem_roaming_scantime`
modem_roaming_scanlist=`nvram get modem_roaming_scanlist`
sim_order=`nvram get modem_sim_order`

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

if [ "$1" == "bytes" -o "$1" == "bytes-" ]; then
	if [ "$modem_dev" == "" ]; then
		echo "Can't get the active network device of USB."
		exit 2
	fi

	if [ -z "$sim_order" ]; then
		echo "Fail to get the SIM order."
		exit 12
	fi

	if [ ! -d "$jffs_dir/sim/$sim_order" ]; then
		mkdir -p "$jffs_dir/sim/$sim_order"
	fi

	rx_new=`cat "/sys/class/net/$modem_dev/statistics/rx_bytes" 2>/dev/null`
	tx_new=`cat "/sys/class/net/$modem_dev/statistics/tx_bytes" 2>/dev/null`
	echo "  rx_new=$rx_new."
	echo "  tx_new=$tx_new."

	if [ "$1" == "bytes" ]; then
		rx_old=`cat "$jffs_dir/sim/$sim_order/modem_bytes_rx" 2>/dev/null`
		if [ -z "$rx_old" ]; then
			rx_old=0
		fi
		tx_old=`cat "$jffs_dir/sim/$sim_order/modem_bytes_tx" 2>/dev/null`
		if [ -z "$tx_old" ]; then
			tx_old=0
		fi
		echo "  rx_old=$rx_old."
		echo "  tx_old=$tx_old."

		rx_reset=`nvram get modem_bytes_rx_reset`
		if [ -z "$rx_reset" ]; then
			rx_reset=0
		fi
		tx_reset=`nvram get modem_bytes_tx_reset`
		if [ -z "$tx_reset" ]; then
			tx_reset=0
		fi
		echo "rx_reset=$rx_reset."
		echo "tx_reset=$tx_reset."

		rx_now=`lplus $rx_old $rx_new`
		tx_now=`lplus $tx_old $tx_new`
		rx_now=`lminus $rx_now $rx_reset`
		tx_now=`lminus $tx_now $tx_reset`
		echo "  rx_now=$rx_now."
		echo "  tx_now=$tx_now."

		echo -n "$rx_now" > "$jffs_dir/sim/$sim_order/modem_bytes_rx"
		nvram set modem_bytes_rx=$rx_now
		echo -n "$tx_now" > "$jffs_dir/sim/$sim_order/modem_bytes_tx"
		nvram set modem_bytes_tx=$tx_now
	else
		echo -n 0 > "$jffs_dir/sim/$sim_order/modem_bytes_rx"
		nvram set modem_bytes_rx=0
		echo -n 0 > "$jffs_dir/sim/$sim_order/modem_bytes_tx"
		nvram set modem_bytes_tx=0
		data_start=`nvram get modem_bytes_data_start 2>/dev/null`
		if [ -n "$data_start" ]; then
			echo -n "$data_start" > "$jffs_dir/sim/$sim_order/modem_bytes_data_start"
		fi
	fi

	nvram set modem_bytes_rx_reset=$rx_new
	nvram set modem_bytes_tx_reset=$tx_new
	echo "set rx_reset=$rx_new."
	echo "set tx_reset=$tx_new."

	echo "done."
elif [ "$1" == "get_dataset" ]; then
	if [ -z "$sim_order" ]; then
		echo "Fail to get the SIM order."
		exit 12
	fi

	echo "Getting data setting..."

	if [ ! -d "$jffs_dir/sim/$sim_order" ]; then
		mkdir -p "$jffs_dir/sim/$sim_order"
	fi

	data_start=`cat "$jffs_dir/sim/$sim_order/modem_bytes_data_start" 2>/dev/null`
	data_cycle=`cat "$jffs_dir/sim/$sim_order/modem_bytes_data_cycle" 2>/dev/null`
	data_limit=`cat "$jffs_dir/sim/$sim_order/modem_bytes_data_limit" 2>/dev/null`
	data_warning=`cat "$jffs_dir/sim/$sim_order/modem_bytes_data_warning" 2>/dev/null`

	if [ -n "$data_start" ]; then
		nvram set modem_bytes_data_start=$data_start
	fi
	if [ -z "$data_cycle" ] || [ "$data_cycle" -lt 1 -o "$data_cycle" -gt 31 ]; then
		data_cycle=1
		echo -n "$data_cycle" > "$jffs_dir/sim/$sim_order/modem_bytes_data_cycle"
	fi
	nvram set modem_bytes_data_cycle=$data_cycle
	if [ -z "$data_limit" ]; then
		data_limit=0
		echo -n "$data_limit" > "$jffs_dir/sim/$sim_order/modem_bytes_data_limit"
	fi
	nvram set modem_bytes_data_limit=$data_limit
	if [ -z "$data_warning" ]; then
		data_warning=0
		echo -n "$data_warning" > "$jffs_dir/sim/$sim_order/modem_bytes_data_warning"
	fi
	nvram set modem_bytes_data_warning=$data_warning

	echo "done."
elif [ "$1" == "set_dataset" ]; then
	if [ -z "$sim_order" ]; then
		echo "Fail to get the SIM order."
		exit 12
	fi

	echo "Setting data setting..."

	if [ ! -d "$jffs_dir/sim/$sim_order" ]; then
		mkdir -p "$jffs_dir/sim/$sim_order"
	fi

	data_start=`nvram get modem_bytes_data_start 2>/dev/null`
	data_cycle=`nvram get modem_bytes_data_cycle 2>/dev/null`
	data_limit=`nvram get modem_bytes_data_limit 2>/dev/null`
	data_warning=`nvram get modem_bytes_data_warning 2>/dev/null`

	if [ -n "$data_start" ]; then
		echo -n "$data_start" > "$jffs_dir/sim/$sim_order/modem_bytes_data_start"
	fi
	if [ -z "$data_cycle" ] || [ "$data_cycle" -lt 1 -o "$data_cycle" -gt 31 ]; then
		data_cycle=1
		nvram set modem_bytes_data_cycle=$data_cycle
	fi
	echo -n "$data_cycle" > "$jffs_dir/sim/$sim_order/modem_bytes_data_cycle"
	if [ -z "$data_limit" ]; then
		data_limit=0
		nvram set modem_bytes_data_limit=$data_limit
	fi
	echo -n "$data_limit" > "$jffs_dir/sim/$sim_order/modem_bytes_data_limit"
	if [ -z "$data_warning" ]; then
		data_warning=0
		nvram set modem_bytes_data_warning=$data_warning
	fi
	echo -n "$data_warning" > "$jffs_dir/sim/$sim_order/modem_bytes_data_warning"

	echo "done."
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
			if [ "$modem_enable" == "2" -a "$sim_inserted3" == "SIM busy" ]; then
				echo "Detected CDMA2000's SIM"
				act_sim=1
			else
				echo "CME ERROR: $sim_inserted3"
				act_sim=-2
			fi
		fi
	else
		echo "No or unknown response."
		act_sim=-10
	fi

	act_sim_orig=`nvram get usb_modem_act_sim`
	if [ "$act_sim_orig" != "$act_sim" ]; then
		nvram set usb_modem_act_sim=$act_sim
	fi

	echo "done."
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

	nvram set usb_modem_act_signal=$signal

	echo "$signal"
	echo "done."
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

		nvram set usb_modem_act_operation="$operation"

		echo "$operation"
		echo "done."
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
		echo "done."
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
		echo "done."
	fi
elif [ "$1" == "imsi" ]; then
	echo "Getting IMSI..."
	at_ret=`$at_lock modem_at.sh '+CIMI' 2>/dev/null`
	ret=`echo "$at_ret" |grep "^[0-9].*$" 2>/dev/null`
	if [ "$ret" == "" ]; then
		echo "Fail to get the IMEI from $modem_act_node."
		exit 11
	fi

	nvram set usb_modem_act_imsi=$ret

	sim_num=`nvram get modem_sim_num`
	if [ -z "$sim_num" ]; then
		sim_num=10
	fi

	nvram set modem_sim_order=-1
	i=1
	while [ $i -le $sim_num ]; do
		echo -n "check SIM($i)..."
		got_imsi=`nvram get modem_sim_imsi$i`

		if [ "$got_imsi" == "" ]; then
			echo "Set SIM($i)."
			nvram set modem_sim_order=$i
			nvram set modem_sim_imsi${i}=$ret
			break
		elif [ "$got_imsi" == "$ret" ]; then
			echo "Get SIM($i)."
			nvram set modem_sim_order=$i
			break
		fi

		i=$((i+1))
	done

	echo "done."
elif [ "$1" == "imsi_del" ]; then
	if [ -z "$2" ]; then
		echo "Usage: $0 $1 <SIM's order>"
		exit 11;
	fi

	echo "Delete SIM..."

	sim_num=`nvram get modem_sim_num`
	if [ -z "$sim_num" ]; then
		sim_num=10
	fi

	i=$2
	while [ $i -le $sim_num ]; do
		echo -n "check SIM($i)..."
		got_imsi=`nvram get modem_sim_imsi$i`

		if [ $i -eq $2 ]; then
			echo -n "Delete SIM($i)."
			got_imsi=""
			nvram set modem_sim_imsi$i=$got_imsi
			rm -rf "$jffs_dir/sim/$i"
		fi

		if [ -z "$got_imsi" ]; then
			j=$((i+1))
			next_imsi=`nvram get modem_sim_imsi$j`
			if [ -n "$next_imsi" ]; then
				echo -n "Move SIM($j) to SIM($i)."
				nvram set modem_sim_imsi$i=$next_imsi
				mv "$jffs_dir/sim/$j" "$jffs_dir/sim/$i"
				nvram set modem_sim_imsi$j=
			fi
		fi

		echo ""

		i=$((i+1))
	done

	echo "done."
elif [ "$1" == "imei" ]; then
	echo -n "Getting IMEI..."
	at_ret=`$at_lock modem_at.sh '+CGSN' 2>/dev/null`
	ret=`echo "$at_ret" |grep "^[0-9].*$" 2>/dev/null`
	if [ "$ret" == "" ]; then
		echo "Fail to get the IMEI from $modem_act_node."
		exit 12
	fi

	nvram set usb_modem_act_imei=$ret

	echo "done."
elif [ "$1" == "iccid" ]; then
	if [ "$modem_vid" == "1478" -a "$modem_pid" == "36902" ]; then
		echo -n "Getting ICCID..."
		at_ret=`$at_lock modem_at.sh '+ICCID' 2>/dev/null`
		ret=`echo "$at_ret" |grep "ICCID: " |awk '{FS="ICCID: "; print $2}' 2>/dev/null`
		if [ "$ret" == "" ]; then
			echo "Fail to get the ICCID from $modem_act_node."
			exit 13
		fi

		nvram set usb_modem_act_iccid=$ret

		echo "done."
	fi
elif [ "$1" == "rate" ]; then
	if [ "$modem_vid" == "1478" -a "$modem_pid" == "36902" ]; then
		echo -n "Getting Rate..."
		qcqmi=`_get_qcqmi_by_usbnet $modem_dev 2>/dev/null`
		at_ret=`gobi_api $qcqmi rate |grep "Max Tx" 2>/dev/null`
		max_tx=`echo "$at_ret" |awk '{FS=","; print $1}' |awk '{FS=" "; print $3}' 2>/dev/null`
		max_rx=`echo "$at_ret" |awk '{FS=","; print $2}' |awk '{FS=" "; print $2}' |awk '{FS="."; print $1}' 2>/dev/null`
		if [ "$max_tx" == "" -o "$max_rx" == "" ]; then
			echo "Fail to get the IMEI from $modem_act_node."
			exit 14
		fi

		nvram set usb_modem_act_tx=$max_tx
		nvram set usb_modem_act_rx=$max_rx

		echo "done."
	fi
elif [ "$1" == "hwver" ]; then
	if [ "$modem_vid" == "1478" -a "$modem_pid" == "36902" ]; then
		echo -n "Getting HWVER..."
		at_ret=`$at_lock modem_at.sh '$HWVER' 2>/dev/null`
		ret=`echo "$at_ret" |grep "^[0-9].*$" 2>/dev/null`
		if [ "$ret" == "" ]; then
			nvram set usb_modem_act_hwver=
			echo "Fail to get the hardware version from $modem_act_node."
			exit 15
		fi

		nvram set usb_modem_act_hwver=$ret

		echo "done."
	fi
elif [ "$1" == "band" ]; then
	if [ "$modem_vid" == "1478" -a "$modem_pid" == "36902" ]; then
		echo -n "Getting Band..."
		at_ret=`$at_lock modem_at.sh '$CRFI' 2>/dev/null`
		ret=`echo "$at_ret" |grep '$CRFI:' |awk '{FS=":"; print $2}' 2>/dev/null`
		if [ "$ret" == "" ]; then
			echo "Fail to get the current band from $modem_act_node."
			exit 16
		fi

		nvram set usb_modem_act_band="$ret"

		echo "done."
	fi
elif [ "$1" == "scan" ]; then
	echo "Start to scan the stations:"
	nvram set usb_modem_act_scanning=2
	at_ret=`$at_lock modem_at.sh '+COPS=2' |grep "OK" 2>/dev/null`

	echo "Scanning the stations."
	at_ret=`$at_lock modem_at.sh '+COPS=?' $modem_roaming_scantime 2>/dev/null`
	ret=`echo "$at_ret" |grep '+COPS: ' |awk '{FS=": "; print $2}' |awk '{FS=",,"; print $1}' 2>/dev/null`
	echo "Finish the scan."
	nvram set usb_modem_act_scanning=1
	if [ "$ret" == "" ]; then
		echo "17:Fail to scan the stations."
		exit 17
	fi

	echo "Count the stations."
	num=`echo "$ret" |awk '{FS=")"; print NF}' 2>/dev/null`
	if [ "$num" == "" ]; then
		echo "18:Fail to count the stations."
		exit 18
	fi

	echo "Work the list."
	list="["
	filter=""
	i=1
	while [ $i -lt $num ]; do
		str=`echo "$ret" |awk '{FS=")"; print $'$i'}' |awk '{FS="("; print $2}' 2>/dev/null`

		sta=`echo "$str" |awk '{FS=","; print $2}' 2>/dev/null`
		sta_code=`echo "$str" |awk '{FS=","; print $4}' 2>/dev/null`
		sta_type_number=`echo "$str" |awk '{FS=","; print $5}' 2>/dev/null`
		if [ "$sta_type_number" == "0" -o "$sta_type_number" == "1" -o "$sta_type_number" == "3" ]; then
			sta_type=2G
		elif [ "$sta_type_number" == "2" ]; then
			sta_type=3G
		elif [ "$sta_type_number" == "4" ]; then
			sta_type=HSDPA
		elif [ "$sta_type_number" == "5" ]; then
			sta_type=HSUPA
		elif [ "$sta_type_number" == "6" ]; then
			sta_type=H+
		elif [ "$sta_type_number" == "7" ]; then
			sta_type=4G
		else
			sta_type=unknown
		fi

		if [ "$list" != "[" ]; then
			list=$list",[$sta, $sta_code, \"$sta_type\"]"
		else
			list=$list"[$sta, $sta_code, \"$sta_type\"]"
		fi
		filter=$filter","$sta","

		i=$((i+1))
	done
	list=$list"]"
	echo -n "$list" > $modem_roaming_scanlist
	nvram set usb_modem_act_scanning=0

	echo "done."
elif [ "$1" == "station" ]; then
	$at_lock modem_at.sh "+COPS=1,0,\"$2\"" 1,2>/dev/null
	if [ $? -ne 0 ]; then
		echo "19:Fail to set the station: $2."
		exit 19
	fi

	echo "done."
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
		echo "done."
	fi
elif [ "$1" == "simpin" ]; then
	if [ "$2" == "" ]; then
		nvram set g3state_pin=2

		echo "24:Need to input the PIN code."
		exit 24
	fi

	nvram set g3state_pin=1
	at_ret=`$at_lock modem_at.sh '+CPIN='\"$2\" |grep "OK" 2>/dev/null`
	if [ "$at_ret" == "" ]; then
		nvram set g3err_pin=1

		echo "25:Fail to unlock the SIM: $2."
		exit 25
	fi

	nvram set g3err_pin=0
	echo "done."
elif [ "$1" == "simpuk" ]; then
	if [ "$2" == "" ]; then
		echo "26:Need to input the PUK code."
		exit 26
	elif [ "$3" == "" ]; then
		echo "27:Need to input the new PIN code."
		exit 27
	fi

	at_ret=`$at_lock modem_at.sh '+CPIN='\"$2\"','\"$3\" |grep "OK" 2>/dev/null`
	if [ "$at_ret" == "" ]; then
		echo "28:Fail to unlock the SIM PIN: $2."
		exit 28
	fi

	echo "done."
elif [ "$1" == "lockpin" ]; then
	simauth=`nvram get usb_modem_act_auth`
	if [ "$simauth" == "1" ]; then
		echo "29:SIM need to input the PIN code first."
		exit 29
	elif [ "$simauth" == "4" -o "$simauth" == "5" ]; then # lock
		echo "30:SIM had been blocked."
		exit 30
	elif [ "$simauth" == "0" ]; then # lock
		echo "31:Can't get the SIM auth state."
		exit 31
	fi

	if [ "$2" == "" ]; then
		echo "32:Decide to lock/unlock PIN."
		exit 32
	fi

	if [ "$3" == "" ]; then
		echo "33:Need the PIN code."
		exit 33
	fi

	if [ "$2" == "1" ]; then # lock
		if [ "$simauth" == "2" ]; then
			echo "had locked."
			echo "done."
			exit 0
		fi

		at_ret=`$at_lock modem_at.sh '+CLCK="SC",1,"'$3'"' |grep "OK" 2>/dev/null`
		if [ "$at_ret" == "2" ]; then
			echo "34:Fail to lock PIN."
			exit 34
		fi
	else
		if [ "$simauth" == "3" ]; then
			echo "had unlocked."
			echo "done."
			exit 0
		fi

		at_ret=`$at_lock modem_at.sh '+CLCK="SC",0,"'$3'"' |grep "OK" 2>/dev/null`
		if [ "$at_ret" == "2" ]; then
			echo "35:Fail to unlock PIN."
			exit 35
		fi
	fi

	echo "done."
elif [ "$1" == "pwdpin" ]; then
	if [ "$2" == "" ]; then
		echo "36:Need to input the original PIN code."
		exit 36
	elif [ "$3" == "" ]; then
		echo "37:Need to input the new PIN code."
		exit 37
	fi

	at_ret=`$at_lock modem_at.sh '+CPWD="SC",'$2','$3 |grep "OK" 2>/dev/null`
	if [ "$at_ret" == "" ]; then
		echo "38:Fail to change the PIN."
		exit 38
	fi

	echo "done."
elif [ "$1" == "gnws" ]; then
	if [ "$modem_vid" == "1478" -a "$modem_pid" == "36902" ]; then
		at_cgnws=`$at_lock modem_at.sh '+CGNWS' |grep "+CGNWS:" |awk '{FS=":"; print $2}' 2>/dev/null`
		if [ "$at_cgnws" == "" ]; then
			echo "Fail to get the CGNWS."
			exit 39
		fi

		roaming=`echo "$at_cgnws" |awk '{FS=","; print $1}' 2>/dev/null`
		signal=`echo "$at_cgnws" |awk '{FS=","; print $2}' 2>/dev/null`
		reg_type=`echo "$at_cgnws" |awk '{FS=","; print $3}' 2>/dev/null`
		reg_state=`echo "$at_cgnws" |awk '{FS=","; print $4}' 2>/dev/null`
		mcc=`echo "$at_cgnws" |awk '{FS=","; print $5}' 2>/dev/null`
		mnc=`echo "$at_cgnws" |awk '{FS=","; print $6}' 2>/dev/null`
		spn=`echo "$at_cgnws" |awk '{FS=","; print $7}' 2>/dev/null`
		isp_long=`echo "$at_cgnws" |awk '{FS=","; print $8}' 2>/dev/null`
		isp_short=`echo "$at_cgnws" |awk '{FS=","; print $9}' 2>/dev/null`

		echo "   Roaming=$roaming."
		echo "    Signal=$signal."
		echo " REG. Type=$reg_type."
		echo "REG. State=$reg_state."
		echo "       MCC=$mcc."
		echo "       MNC=$mnc."
		echo "       SPN=$spn."
		echo "  ISP Long=$isp_long."
		echo " ISP Short=$isp_short."
		echo "done."
	fi
fi


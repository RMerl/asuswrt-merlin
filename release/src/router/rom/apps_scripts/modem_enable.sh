#!/bin/sh
# echo "This is a script to enable the modem."


modem_enable=`nvram get modem_enable`
modem_mode=`nvram get modem_mode`
modem_roaming=`nvram get modem_roaming`
modem_roaming_mode=`nvram get modem_roaming_mode`
modem_roaming_isp=`nvram get modem_roaming_isp`
modem_roaming_imsi=`nvram get modem_roaming_imsi`
modem_autoapn=`nvram get modem_autoapn`
modem_auto_spn=`nvram get usb_modem_auto_spn`
modem_act_path=`nvram get usb_modem_act_path`
modem_type=`nvram get usb_modem_act_type`
act_node1="usb_modem_act_int"
act_node2="usb_modem_act_bulk"
modem_vid=`nvram get usb_modem_act_vid`
modem_pid=`nvram get usb_modem_act_pid`
modem_dev=`nvram get usb_modem_act_dev`
modem_imsi=`nvram get usb_modem_act_imsi`
modem_pin=`nvram get modem_pincode`
modem_pdp=`nvram get modem_pdp`
modem_isp=`nvram get modem_isp`
modem_spn=`nvram get modem_spn`
modem_apn=`nvram get modem_apn`
modem_authmode=`nvram get modem_authmode`
modem_user=`nvram get modem_user`
modem_pass=`nvram get modem_pass`
modem_apn_v6=`nvram get modem_apn_v6`
modem_authmode_v6=`nvram get modem_authmode_v6`
modem_user_v6=`nvram get modem_user_v6`
modem_pass_v6=`nvram get modem_pass_v6`
modem_reg_time=`nvram get modem_reg_time`

at_lock="flock -x /tmp/at_cmd_lock"
pdp_old=0


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

_find_usb3_path(){
	all_paths=`nvram get xhci_ports`

	count=1
	for path in $all_paths; do
		len=${#path}
		target=`echo $1 |head -c $len`
		if [ "$target" == "$path" ]; then
			echo "$count"
			return
		fi

		count=$((count+1))
	done

	echo "-1"
}

_find_usb2_path(){
	all_paths=`nvram get ehci_ports`

	count=1
	for path in $all_paths; do
		len=${#path}
		target=`echo $1 |head -c $len`
		if [ "$target" == "$path" ]; then
			echo "$count"
			return
		fi

		count=$((count+1))
	done

	echo "-1"
}

_find_usb1_path(){
	all_paths=`nvram get ohci_ports`

	count=1
	for path in $all_paths; do
		len=${#path}
		target=`echo $1 |head -c $len`
		if [ "$target" == "$path" ]; then
			echo "$count"
			return
		fi

		count=$((count+1))
	done

	echo "-1"
}

_find_usb_path(){
	ret=`_find_usb3_path "$1"`
	if [ "$ret" == "-1" ]; then
		ret=`_find_usb2_path "$1"`
		if [ "$ret" == "-1" ]; then
			ret=`_find_usb1_path "$1"`
		fi
	fi

	echo "$ret"
}


if [ "$modem_type" == "" ]; then
	find_modem_type.sh

	modem_type=`nvram get usb_modem_act_type`
	if [ "$modem_type" == "" ]; then
		echo "Can't get usb_modem_act_type!"
		exit 1
	fi
fi

if [ "$modem_enable" == "0" ]; then
	echo "Don't enable the USB Modem."
	exit 0
elif [ "$modem_enable" == "4" ]; then
	echo "Running the WiMAX procedure..."
	exit 0
fi

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
		exit 2
	fi
fi

echo "VAR: modem_enable($modem_enable) modem_autoapn($modem_autoapn) modem_act_node($modem_act_node) modem_type($modem_type) modem_vid($modem_vid) modem_pid($modem_pid)";
echo "     modem_isp($modem_isp) modem_apn($modem_apn)";

if [ "$modem_type" == "tty" -o "$modem_type" == "qmi" -o "$modem_type" == "mbim" -o "$modem_type" == "gobi" ]; then
	nvram_reset=`nvram get modem_act_reset`
	#if [ "$nvram_reset" == "1" -o "$modem_vid" == "6610" -a "$modem_pid" == "644" ]; then # ZTE MF880
	if [ "$nvram_reset" == "1" ]; then
		# Reset modem.
		echo "Reset modem."
		nvram set usb_modem_act_reset=1
		nvram set usb_modem_act_reset_path="$modem_act_path"
		at_ret=`$at_lock modem_at.sh '+CFUN=1,1' |grep "OK" 2>/dev/null`
		if [ "$at_ret" == "OK" ]; then
			tries=1
			while [ $tries -le 30 -a "$modem_act_node" != "" ]; do
				echo "Reset modem: wait the modem to start reseting...$tries"
				sleep 1

				modem_act_node=`nvram get $act_node`
				tries=$((tries+1))
			done
			if [ "$modem_act_node" != "" ]; then
				echo "Reset modem: Fail to reset modem."
				nvram set usb_modem_act_reset=0
				nvram set usb_modem_act_reset_path=""
				exit 5
			fi

			echo "Reset modem: wait the modem to wake up..."
			reset_flag=`nvram get usb_modem_act_reset`
			tries=1
			while [ $tries -le 30 -a "$reset_flag" != "2" ]; do
				echo "Reset modem: wait the modem to wake up...$tries"
				sleep 1

				reset_flag=`nvram get usb_modem_act_reset`
				tries=$((tries+1))
			done

			nvram set usb_modem_act_reset=0
			nvram set usb_modem_act_reset_path=""
			if [ "$reset_flag" != "2" ]; then
				echo "Reset modem: modem can't wake up after reset."
				exit 5
			else
				echo "Reset modem: modem had woken up after reset."
				if [ "$modem_type" == "qmi" ]; then
					echo "Reset Sleep 3 seconds to wait modem nodes."
					sleep 3
				else
					echo "Reset Sleep 2 seconds to wait modem nodes."
					sleep 2
				fi
			fi

			find_modem_node.sh

			modem_act_node=`nvram get $act_node`
			if [ "$modem_act_node" == "" ]; then
				echo "Reset modem: Can't get usb_modem_act_int after reset."
				exit 2.1
			fi
		else
			echo "Reset modem: Can't reset modem."
			nvram set usb_modem_act_reset=0
			nvram set usb_modem_act_reset_path=""
			exit 5
		fi
	fi

	# Set full functionality
	at_ret=`$at_lock modem_at.sh '+CFUN?' |grep "+CFUN: 1" 2>/dev/null`
	if [ "$at_ret" == "" ]; then
		echo "CFUN: Set full functionality."
		at_ret=`$at_lock modem_at.sh '+CFUN=1' |grep "OK" 2>/dev/null`
		if [ "$at_ret" == "OK" ]; then
			tries=1
			at_ret=""
			while [ $tries -le 30 -a "$at_ret" == "" ]; do
				echo "CFUN: wait for setting CFUN...$tries"
				sleep 1

				at_ret=`$at_lock modem_at.sh '+CFUN?' |grep "+CFUN: 1" 2>/dev/null`
				tries=$((tries+1))
			done

			if [ "$at_ret" == "" ]; then
				echo "CFUN: Fail to set full functionality."
				exit 4
			fi
		else
			echo "CFUN: Fail to set +CFUN=1."
			exit 5
		fi
	fi

	# input PIN if need.
	echo "PIN: detect PIN if it's needed."
	modem_status.sh sim
	modem_status.sh simauth
	ret=`nvram get usb_modem_act_sim`
	if [ "$ret" != "1" ]; then
		if [ "$ret" == "2" -a "$modem_pin" != "" ]; then
			echo "Input the PIN code..."
			modem_status.sh simpin "$modem_pin"
			sleep 1

			modem_status.sh sim
			modem_status.sh simauth
			ret=`nvram get usb_modem_act_sim`
		fi

		if [ "$ret" != "1" ]; then
			echo "Incorrect SIM card or can't input the correct PIN/PUK code."
			exit 3
		fi
	fi

	modem_status.sh imsi
	modem_status.sh iccid

	# Auto-APN
	if [ "$modem_autoapn" != "" -a "$modem_autoapn" != "0" -a "$modem_auto_spn" == "" ]; then
		echo "Running autoapn..."
		modem_autoapn.sh

		modem_isp=`nvram get modem_isp`
		modem_spn=`nvram get modem_spn`
		modem_apn=`nvram get modem_apn`
		modem_user=`nvram get modem_user`
		modem_pass=`nvram get modem_pass`
	fi

	if [ "$modem_type" == "gobi" ]; then
		qcqmi=`_get_qcqmi_by_usbnet $modem_dev`
		echo "Got qcqmi: $qcqmi."

		cmd_pipe="/tmp/pipe"

		gobi_pid=`pidof gobi`
		if [ "$gobi_pid" != "" ]; then
			# connect to GobiNet.
			echo -n "1,$qcqmi" >> $cmd_pipe
			sleep 2

			# WDS stop the data session
			echo -n "4" >> $cmd_pipe
			sleep 1

			echo -n "12" >> $cmd_pipe
			sleep 1

			# disconnect to GobiNet.
			echo -n "2" >> $cmd_pipe
			sleep 1

			#echo -n "99" >> $cmd_pipe
			#sleep 1
		fi

		echo "Gobi($qcqmi): set the ISP profile."
		if [ "$gobi_pid" == "" ]; then
			gobi d &
			sleep 1
		fi

		# connect to GobiNet.
		echo -n "1,$qcqmi" >> $cmd_pipe
		sleep 2

		# WDS set the autoconnect & roaming
		# autoconnect: 0, disable; 1, enable; 2, pause.
		# roaming: 0, allow; 1, disable. Only be activated when autoconnect is enabled.
		echo "Pause the connection."
		if [ "$modem_roaming" != "1" ]; then
			echo "Disable roaming."
			echo -n "7,2,1" >> $cmd_pipe
		else
			echo "Enable roaming."
			echo -n "7,2,0" >> $cmd_pipe
		fi
		sleep 1

		if [ "$pdp_old" -eq "1" ]; then
			# WDS start the data session
			if [ "$modem_pdp" -eq "1" ]; then
				# PPP
				echo -n "11,8,$modem_apn,$modem_user,$modem_pass" >> $cmd_pipe
			elif [ "$modem_pdp" -eq "2" ]; then
				# IPv6
				echo -n "11,6,$modem_apn_v6,$modem_user_v6,$modem_pass_v6" >> $cmd_pipe
			elif [ "$modem_pdp" -eq "3" ]; then
				# IPv4v6
				echo -n "11,4,$modem_apn,$modem_user,$modem_pass" >> $cmd_pipe
				sleep 3
				echo -n "11,6,$modem_apn_v6,$modem_user_v6,$modem_pass_v6" >> $cmd_pipe
			else
				# IPv4
				echo -n "11,4,$modem_apn,$modem_user,$modem_pass" >> $cmd_pipe
			fi
			sleep 3
		else
			# set the default profile to auto-connect.
			if [ -z "$modem_isp" ]; then
				modem_isp="space"
			fi
			echo -n "5,$modem_pdp,$modem_isp,$modem_apn,$modem_authmode,$modem_user,$modem_pass" >> $cmd_pipe
			sleep 1
		fi

		echo "Gobi: Successfull to set the ISP profile."
	else
		echo "$modem_type: set the ISP profile."
		at_ret=`$at_lock modem_at.sh "+CGDCONT=1,\"IP\",\"$modem_apn\"" |grep "OK" 2>/dev/null`
		if [ "$at_ret" != "OK" ]; then
			echo "$modem_type: Fail to set the profile."
			exit 0
		fi
	fi

	# set COPS.
	at_ret=`$at_lock modem_at.sh '+COPS?' |grep "OK" 2>/dev/null`
	if [ "$at_ret" == "OK" ]; then
		echo "COPS: Can execute +COPS..."

		if [ "$modem_vid" == "6797" -a "$modem_pid" == "4098" ]; then # BandLuxe C120.
			echo "COPS: BandLuxe C120 start with CFUN=0, so don't need to unregister the network."
		else
			at_ret=`$at_lock modem_at.sh '+COPS=2' "$modem_reg_time" |grep "OK" 2>/dev/null`
			if [ "$at_ret" != "OK" ]; then
				echo "Can't deregister from network."
				exit 6
			fi
		fi

		# Home service.
		if [ "$modem_roaming" != "1" ]; then
			echo "COPS: set +COPS=0."
			at_ret=`$at_lock modem_at.sh '+COPS=0' "$modem_reg_time" |grep "OK" 2>/dev/null`
			if [ "$at_ret" != "OK" ]; then
				echo "COPS: Fail to set +COPS=0."
				exit 6
			fi
		elif [ "$modem_roaming_mode" == "1" ]; then
			# roaming manually...
			echo "roaming manually..."
			if [ -n "$modem_roaming_isp" ]; then
				modem_status.sh station "$modem_roaming_isp"
			fi
			# Don't need to change the modem settings.
			#modem_autoapn.sh set $modem_roaming_imsi

			#modem_isp=`nvram get modem_isp`
			#modem_spn=`nvram get modem_spn`
			#modem_apn=`nvram get modem_apn`
			#modem_user=`nvram get modem_user`
			#modem_pass=`nvram get modem_pass`
		else
			# roaming automatically...
			echo "roaming automatically..."
			at_ret=`$at_lock modem_at.sh '+COPS=0' "$modem_reg_time" |grep "OK" 2>/dev/null`
			if [ "$at_ret" != "OK" ]; then
				echo "COPS: Fail to set +COPS=0 to roam automatically."
				exit 6
			fi
		fi
	else # the result from CDMA2000 can be "COMMAND NOT SUPPORT", "ERROR".
		echo "COPS: Don't support +COPS."
	fi

	modem_status.sh setmode $modem_mode

	# check the register state after set COPS.
	at_ret=`$at_lock modem_at.sh '+CGATT?' 2>/dev/null`
	ret=`echo -n "$at_ret" |grep "OK"`
	if [ "$ret" == "OK" ]; then
		echo "CGATT: 1. Check the register state..."
		tries=1
		at_ret=`echo -n "$at_ret" |grep "+CGATT: 1"`
		while [ $tries -le 30 -a "$at_ret" == "" ]; do
			echo "CGATT: wait for network registered...$tries"
			sleep 1

			at_ret=`$at_lock modem_at.sh '+CGATT?' |grep "+CGATT: 1" 2>/dev/null`
			tries=$((tries+1))
		done

		if [ "$at_ret" == "" ]; then
			echo "CGATT: Fail to register network, please check."
			exit 7
		else
			echo "CGATT: Successfull to register network."
		fi
	else # the result from CDMA2000 can be "COMMAND NOT SUPPORT", "ERROR".
		echo "CGATT: Don't support +CGATT."
	fi

	if [ "$modem_vid" == "8193" ];then
		# just for D-Link DWM-156 A7.
		#at_ret=`$at_lock modem_at.sh '+BMHPLMN' |grep "+BMHPLMN: " 2>/dev/null`
		#if [ "$at_ret" != "" ]; then
		#	plmn=`echo $at_ret | awk '{print $2}'`
		#	echo "IMSI: $plmn."
		#fi

		# put the dongle in the general procedure.
		exit 0

		at_ret=`$at_lock modem_at.sh '+CFUN=1' |grep "OK" 2>/dev/null`
		if [ "$at_ret" != "OK" ]; then
			echo "Fail to set CFUN=1."
			exit -1
		fi

		tries=1
		at_ret=""
		while [ $tries -le 30 -a "$at_ret" == "" ]; do
			echo "wait for network registered...$tries"
			sleep 1

			at_ret=`$at_lock modem_at.sh '+CGATT?' |grep "+CGATT: 1" 2>/dev/null`
			tries=$((tries+1))
		done

		if [ "$at_ret" == "" ]; then
			echo "Fail to register network, please check."
			exit -1
		fi

		echo "Successfull to register network."
	elif [ "$modem_type" == "qmi" ]; then
		if [ "$modem_dev" == "" ]; then
			path=`_find_usb_path "$modem_act_path"`
			modem_dev=`nvram get usb_path"$path"_act`
			if [ "$modem_dev" == "" ]; then
				echo "Couldn't get the QMI dev..."
				exit 8
			fi

			nvram set usb_modem_act_dev=$modem_dev
			echo "Got the QMI dev: $modem_dev."
		fi

		wdm=`_get_wdm_by_usbnet $modem_dev`

		if [ "$modem_user" != "" -o "$modem_pass" != "" ]; then
			if [ "$modem_authmode" == "3" ]; then
				flag_auth="--auth-type both"
			elif [ "$modem_authmode" == "2" ]; then
				flag_auth="--auth-type chap"
			elif [ "$modem_authmode" == "1" ]; then
				flag_auth="--auth-type pap"
			else
				flag_auth="--auth-type none"
			fi

			if [ "$modem_user" != "" ]; then
				flag_auth="$flag_auth --username $modem_user"
			fi
			if [ "$modem_pass" != "" ]; then
				flag_auth="$flag_auth --password $modem_pass"
			fi
		else
			flag_auth=""
		fi

		echo "QMI($wdm): set the ISP profile."
		tries=1
		ret=1
		while [ $tries -le 6 -a "$ret" != "0" ]; do
			echo "QMI: set the ISP profile $tries times..."
			uqmi -d $wdm --keep-client-id wds --start-network $modem_apn $flag_auth
			ret=$?

			sleep 1
			tries=$((tries+1))
		done

		if [ "$ret" != "0" ]; then
			echo "QMI: Fail to set the profile."
			exit 0
		elif [ "$modem_vid" == "4817" -a "$modem_pid" == "5132" ]; then
			# put the dongle in the general procedure.
			exit 0

			echo "Restarting the MT and set it as online mode..."
			nvram set usb_modem_reset_huawei=1
			at_ret=`$at_lock modem_at.sh '+CFUN=1,1'`

			tries=1
			at_ret=""
			while [ $tries -le 30 -a "$at_ret" == "" ]; do
				echo "wait the modem to wake up...$tries"
				sleep 1

				at_ret=`$at_lock modem_at.sh '+CGATT?' |grep "+CGATT: 1" 2>/dev/null`
				tries=$((tries+1))
			done

			if [ "$ret" == "" ]; then
				echo "Fail to reset the modem, please check."
				exit -1
			fi

			echo "Successfull to reset the modem."
			nvram unset usb_modem_reset_huawei
		fi
		echo "QMI: Successfull to set the ISP profile."
	elif [ "$modem_type" == "gobi" ]; then
		# WDS set the autoconnect & roaming
		# autoconnect: 0, disable; 1, enable; 2, pause.
		# roaming: 0, allow; 1, disable. Only be activated when autoconnect is enabled.
		echo "Connect the line automatically."
		if [ "$modem_roaming" != "1" ]; then
			echo "Disable roaming."
			echo -n "7,1,1" >> $cmd_pipe
		elif [ "$modem_roaming_mode" == "1" ]; then
			echo "roaming manually..."
			echo -n "7,1,0" >> $cmd_pipe
		else
			echo "roaming automatically..."
			echo -n "7,1,0" >> $cmd_pipe
		fi
		sleep 1

		echo -n "99" >> $cmd_pipe
		sleep 1

		modem_status.sh rate
		modem_status.sh band

		at_cgnws=`$at_lock modem_at.sh '+CGNWS' |grep "+CGNWS:" |awk '{FS=":"; print $2}' 2>/dev/null`
		if [ "$at_cgnws" != "" ]; then
			mcc=`echo -n "$at_cgnws" |awk '{FS=","; print $5}' 2>/dev/null`
			mnc=`echo -n "$at_cgnws" |awk '{FS=","; print $6}' 2>/dev/null`
			target=$mcc$mnc
			len=${#target}
			target=`echo -n $modem_imsi |cut -c '1-'$len 2>/dev/null`

			if [ "$mcc$mnc" == "$target" ]; then
				spn=`echo -n "$at_cgnws" |awk '{FS=","; print $7}' 2>/dev/null`
				if [ "$modem_spn" == "" -a "$spn" != "" -a "$spn" != "NULL" ]; then
					nvram set modem_spn=$spn
				fi

				# useless temparily.
				#isp=`echo -n "$at_cgnws" |awk '{FS=","; print $8}' 2>/dev/null` # ISP long name
				#if [ "$isp" != "" -a "$isp" != "NULL" ]; then
				#	nvram set modem_isp=$isp
				#else
				#	isp=`echo -n "$at_cgnws" |awk '{FS=","; print $9}' 2>/dev/null` # ISP short name
				#	if [ "$isp" != "" -a "$isp" != "NULL" ]; then
				#		nvram set modem_isp=$isp
				#	fi
				#fi
			fi
		fi
	fi

	if [ "$modem_type" == "qmi" -o "$modem_type" == "gobi" ]; then
		at_ret=`$at_lock modem_at.sh '+CGATT?' 2>/dev/null`
		ret=`echo -n "$at_ret" |grep "OK"`
		if [ "$ret" == "OK" ]; then
			echo "CGATT: 2. Check the register state..."
			tries=1
			at_ret=`echo -n "$at_ret" |grep "+CGATT: 1"`
			while [ $tries -le 30 -a "$at_ret" == "" ]; do
				echo "CGATT: wait for network registered...$tries"
				sleep 1

				at_ret=`$at_lock modem_at.sh '+CGATT?' |grep "+CGATT: 1" 2>/dev/null`
				tries=$((tries+1))
			done

			if [ "$at_ret" == "" ]; then
				echo "CGATT: Fail to register network, please check."
				exit 7
			else
				echo "CGATT: Successfull to register network."
			fi
		else # the result from CDMA2000 can be "COMMAND NOT SUPPORT", "ERROR".
			echo "CGATT: Don't support +CGATT."
		fi
	fi

	echo "modem_enable: done."
fi


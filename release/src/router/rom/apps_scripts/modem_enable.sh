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
wandog_interval=`nvram get wandog_interval`
modem_bridge=`nvram get modem_bridge`
atcmd=`nvram get modem_atcmd`

usb_gobi2=`nvram get usb_gobi2`
Dev3G=`nvram get Dev3G`
kernel_version=`uname -r`


# $1: ifname.
_get_wdm_by_usbnet(){
	rp1=`readlink -f /sys/class/net/$1/device 2>/dev/null`
	if [ "$rp1" == "" ]; then
		echo ""
		return
	fi

	rp2=
	i=0
	while [ $i -lt 5 ]; do
		ver_head=`echo -n $kernel_version |awk 'BEGIN{FS="."}{print $1}'`
		if [ "$ver_head" -ge "4" ]; then
			rp2=`readlink -f /sys/class/usbmisc/cdc-wdm$i/device 2>/dev/null`
		else
			rp2=`readlink -f /sys/class/usb/cdc-wdm$i/device 2>/dev/null`
		fi
		if [ "$rp2" == "" ]; then
			i=`expr $i + 1`
			continue
		fi

		rp1_head=`echo -n $rp1 |awk 'BEGIN{FS=":"}{print $1}'`
		rp2_head=`echo -n $rp2 |awk 'BEGIN{FS=":"}{print $1}'`
		if [ "$rp1_head" == "$rp2_head" ]; then
			echo "/dev/cdc-wdm$i"
			return
		fi

		i=`expr $i + 1`
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
	while [ $i -lt 5 ]; do
		rp2=`readlink -f /sys/class/GobiQMI/qcqmi$i/device 2>/dev/null`
		if [ "$rp2" == "" ]; then
			i=`expr $i + 1`
			continue
		fi

		if [ "$rp1" == "$rp2" ]; then
			echo "qcqmi$i"
			return
		fi

		i=`expr $i + 1`
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

		count=`expr $count + 1`
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

		count=`expr $count + 1`
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

		count=`expr $count + 1`
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

_get_pdp_str(){
	str=""

	if [ "$modem_pdp" -eq "1" ]; then
		str="PPP"
	elif [ "$modem_pdp" -eq "2" ]; then
		str="IPV6"
	elif [ "$modem_pdp" -eq "3" ]; then
		str="IPV4V6"
	else
		str="IP"
	fi

	echo "$str"
}

_get_qmi_pdp_str(){
	str=""

	if [ "$modem_pdp" -eq "1" ]; then
		str="unspecified"
	elif [ "$modem_pdp" -eq "2" ]; then
		str="ipv6"
	elif [ "$modem_pdp" -eq "3" ]; then
		str="unspecified"
	else
		str="ipv4"
	fi

	echo "$str"
}

_is_Docomo_modem(){
	ret="0"

	if [ "$modem_vid" == "4100" -a "$modem_pid" == "25446" ]; then # Docomo L03F.
		ret="1"
	elif [ "$modem_vid" == "4100" -a "$modem_pid" == "25382" ]; then # Docomo L-03D
		ret="1"
	fi

	echo -n "$ret"
}


if [ "$modem_type" == "" ]; then
	/usr/sbin/find_modem_type.sh

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

echo "VAR: modem_enable($modem_enable) modem_autoapn($modem_autoapn)";
echo "     modem_type($modem_type) modem_vid($modem_vid) modem_pid($modem_pid)";
echo "     modem_isp($modem_isp) modem_apn($modem_apn)";

if [ "$modem_type" == "tty" -o "$modem_type" == "qmi" -o "$modem_type" == "mbim" -o "$modem_type" == "gobi" ] || [ "$usb_gobi2" == "1" ]; then
	nvram_reset=`nvram get modem_act_reset`
	#if [ "$nvram_reset" == "1" -o "$modem_vid" == "6610" -a "$modem_pid" == "644" ]; then # ZTE MF880
	if [ "$nvram_reset" == "1" ]; then
		# Reset modem.
		echo "Reset modem."
		nvram set usb_modem_act_reset=1
		nvram set usb_modem_act_reset_path="$modem_act_path"
		at_ret=`/usr/sbin/modem_at.sh '+CFUN=1,1' 2>&1`
		ret=`echo -n $at_ret |grep "OK" 2>/dev/null`
		if [ -n "$ret" ]; then
			tries=1
			modem_act_node=`nvram get $act_node`
			while [ $tries -le 30 ] && [ -n "$modem_act_node" ]; do
				echo "Reset modem: wait the modem to start reseting...$tries"
				sleep 1

				modem_act_node=`nvram get $act_node`
				tries=`expr $tries + 1`
			done
			if [ -n "$modem_act_node" ]; then
				echo "Reset modem: Fail to reset modem."
				nvram set usb_modem_act_reset=0
				nvram set usb_modem_act_reset_path=""
				exit 5
			fi

			echo "Reset modem: wait the modem to wake up..."
			tries=1
			reset_flag=`nvram get usb_modem_act_reset`
			while [ $tries -le 30 ] && [ "$reset_flag" -ne "2" ]; do
				echo "Reset modem: wait the modem to wake up...$tries"
				sleep 1

				reset_flag=`nvram get usb_modem_act_reset`
				tries=`expr $tries + 1`
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
		else
			echo "Reset modem: Can't reset modem."
			nvram set usb_modem_act_reset=0
			nvram set usb_modem_act_reset_path=""
			exit 5
		fi
	fi

	/usr/sbin/find_modem_node.sh

	modem_act_node=`nvram get $act_node`
	if [ "$modem_act_node" == "" ]; then
		echo "Reset modem: Can't get usb_modem_act_int."
		exit 2.1
	else
		echo "Got the int node: $modem_act_node."
	fi

	if [ "$modem_enable" != "2" ]; then
		# Set full functionality
		at_ret=`/usr/sbin/modem_at.sh '+CFUN?' 2>&1`

		if [ "$modem_type" == "gobi" ]; then
			ret=`echo -n $at_ret |grep "+CFUN: 7" 2>/dev/null`
			if [ -n "$ret" ]; then
				reboot_flag=`nvram get usb_modem_act_reboot`
				if [ "$reboot_flag" != "1" ]; then
					logger "Detecct the gobi in phone's mode, so reboot again..."
					nvram set usb_modem_act_reboot=1
					nvram commit
					sleep 2
					reboot
					exit 0
				else
					logger "Detecct the gobi in phone's mode again. Skip to reboot..."
					exit 0
				fi
			else
				nvram unset usb_modem_act_reboot
				nvram commit
			fi
		fi

		ret=`echo -n $at_ret |grep "+CFUN: 1" 2>/dev/null`
		if [ -z "$ret" ]; then
			echo "CFUN: Set full functionality."
			at_ret=`/usr/sbin/modem_at.sh '+CFUN=1' 2>&1`
			ret=`echo -n $at_ret |grep "OK" 2>/dev/null`
			if [ -n "$ret" ]; then
				tries=1
				ret=""
				while [ $tries -le 30 ] && [ -z "$ret" ]; do
					echo "CFUN: wait for setting CFUN...$tries"
					sleep 1

					at_ret=`/usr/sbin/modem_at.sh '+CFUN?' 2>&1`
					ret=`echo -n $at_ret |grep "+CFUN: 1" 2>/dev/null`
					tries=`expr $tries + 1`
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
	fi

	# input PIN if need.
	echo "PIN: detect PIN if it's needed."
	/usr/sbin/modem_status.sh sim
	/usr/sbin/modem_status.sh simauth
	ret=`nvram get usb_modem_act_sim`
	if [ "$ret" != "1" ]; then
		if [ "$ret" == "2" -a "$modem_pin" != "" ]; then
			echo "Input the PIN code..."
			/usr/sbin/modem_status.sh simpin "$modem_pin"
			sleep 1

			/usr/sbin/modem_status.sh sim
			/usr/sbin/modem_status.sh simauth
			ret=`nvram get usb_modem_act_sim`
		fi

		if [ "$ret" != "1" ]; then
			echo "Incorrect SIM card or can't input the correct PIN/PUK code."
			exit 3
		fi
	fi

	if [ "$modem_enable" != "2" ]; then # Don't get IMSI with CDMA2000.
		/usr/sbin/modem_status.sh imsi
		/usr/sbin/modem_status.sh iccid

		# Auto-APN
		if [ "$modem_autoapn" != "" -a "$modem_autoapn" != "0" -a "$modem_auto_spn" == "" ]; then
			echo "Running autoapn..."
			/usr/sbin/modem_autoapn.sh

			modem_isp=`nvram get modem_isp`
			modem_spn=`nvram get modem_spn`
			modem_apn=`nvram get modem_apn`
			modem_user=`nvram get modem_user`
			modem_pass=`nvram get modem_pass`
		fi
	fi

	if [ "$modem_enable" == "2" ]; then
		echo "$modem_type: CDMA2000 skip to set the ISP profile."
	elif [ "$modem_type" == "gobi" ]; then
		if [ "$usb_gobi2" == "1" ]; then
			echo "Gobi2: Pause the autoconnect."
			at_ret=`/usr/sbin/modem_at.sh '+CAUTOCONNECT=0' 2>&1`
			ret=`echo -n $at_ret |grep "OK" 2>/dev/null`
			if [ -z "$ret" ]; then
				echo "Gobi2: Fail to stop the autoconnect."
				exit 0
			fi
			at_ret=`/usr/sbin/modem_at.sh '+CWWAN=0' 2>&1`
			ret=`echo -n $at_ret |grep "OK" 2>/dev/null`
			if [ -z "$ret" ]; then
				echo "Gobi2: Fail to stop the connection."
				exit 0
			fi

			pdp_str=`_get_pdp_str`
			echo "Gobi2: set the PDP be $pdp_str & APN be $modem_apn."
			at_ret=`/usr/sbin/modem_at.sh '+CGDCONT=1,"'$pdp_str'","'$modem_apn'"' 2>&1`
			ret=`echo -n $at_ret |grep "OK" 2>/dev/null`
			if [ -z "$ret" ]; then
				echo "Gobi2: Fail to set the APN profile."
				exit 0
			fi

			if [ "$modem_user" != "" -o "$modem_pass" != "" ]; then
				echo "Gobi2: Set the PPP profile."
				#if [ "$modem_authmode" == "3" ]; then
				#	modem_authmode=2 # When auth type is BOTH, choose CHAP.
				#fi
				#at_ret=`/usr/sbin/modem_at.sh '$QCPDPP=1,'$modem_authmode',"'$modem_pass'","'$modem_user'"' 2>&1`
				at_ret=`/usr/sbin/modem_at.sh '$PPP='$modem_user','$modem_pass 2>&1`
				ret=`echo -n $at_ret |grep "OK" 2>/dev/null`
				if [ -z "$ret" ]; then
					echo "Gobi2: Fail to set the PPP profile."
					exit 0
				fi
			fi
		else
			qcqmi=`_get_qcqmi_by_usbnet $modem_dev`
			echo "Got qcqmi: $qcqmi."

			echo "Pause the connection."
			# WDS set the autoconnect & roaming
			# autoconnect: 0, disable; 1, enable; 2, pause.
			# roaming: 0, allow; 1, disable. Only be activated when autoconnect is enabled.
			gobi_api $qcqmi SetEnhancedAutoconnect 0 1

			echo "Gobi: set +COPS=2."
			wait_time1=`expr $wandog_interval + $wandog_interval`
			wait_time=`expr $wait_time1 + $modem_reg_time`
			nvram set freeze_duck=$wait_time
			/usr/sbin/modem_at.sh '+COPS=2' "$modem_reg_time"
			if [ -z "$atcmd" ] || [ "$atcmd" != "1" ]; then
				/usr/sbin/modem_at.sh '' # clean the output of +COPS=2.
			fi

			echo "Gobi($qcqmi): set the ISP profile."
			# SetDefaultProfile can replace with +CGDCONT.
			gobi_api $qcqmi SetDefaultProfile "$modem_pdp" "$modem_isp" "$modem_apn" "$modem_authmode" "$modem_user" "$modem_pass"
		fi

		echo "Gobi: Successfull to set the ISP profile."
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

		echo "QMI($wdm): set the ISP profile."
		pdp_str=`_get_qmi_pdp_str`
		echo "$modem_type: set the PDP be $pdp_str."
		#at_ret=`/usr/sbin/modem_at.sh '+CGDCONT=1,"'$pdp_str'","'$modem_apn'"' 2>&1`
		#ret=`echo -n $at_ret |grep "OK" 2>/dev/null`
		#if [ -z "$ret" ]; then
		#	echo "$modem_type: Fail to set the profile."
		#	exit 0
		#fi

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

			echo "$modem_type: set the flag_auth be \"$flag_auth\"."

			echo "uqmi -d $wdm --keep-client-id wds --start-network \"$modem_apn\" $flag_auth --autoconnect"
			uqmi -d $wdm --keep-client-id wds --start-network "$modem_apn" $flag_auth --autoconnect
		else
			echo "uqmi -d $wdm --keep-client-id wds --start-network \"$modem_apn\" --autoconnect"
			uqmi -d $wdm --keep-client-id wds --start-network "$modem_apn" --autoconnect
		fi

		if [ "$?" != "0" ]; then
			echo "QMI($wdm): faile to start the network & enable autoconnect..."
		fi

		if [ "$modem_vid" == "4817" -a "$modem_pid" == "5132" ]; then
			# put the dongle in the general procedure.
			exit 0

			echo "Restarting the MT and set it as online mode..."
			nvram set usb_modem_reset_huawei=1
			at_ret=`/usr/sbin/modem_at.sh '+CFUN=1,1' 2>&1`

			tries=1
			ret=""
			while [ $tries -le 30 ] && [ -z "$ret" ]; do
				echo "wait the modem to wake up...$tries"
				sleep 1

				at_ret=`/usr/sbin/modem_at.sh '+CGATT?' 2>&1`
				ret=`echo -n $at_ret |grep "+CGATT: 1" 2>/dev/null`
				tries=`expr $tries + 1`
			done

			if [ "$ret" == "" ]; then
				echo "Fail to reset the modem, please check."
				exit -1
			fi

			echo "Successfull to reset the modem."
			nvram unset usb_modem_reset_huawei
		fi

		echo "QMI: Successfull to set the ISP profile."
	elif [ -n "$modem_apn" ]; then
		echo "$modem_type: set the ISP profile."
		pdp_str=`_get_pdp_str`
		echo "$modem_type: set the PDP be $pdp_str."
		at_ret=`/usr/sbin/modem_at.sh '+CGDCONT=1,"'$pdp_str'","'$modem_apn'"' 2>&1`
		ret=`echo -n $at_ret |grep "OK" 2>/dev/null`
		if [ -z "$ret" ]; then
			echo "$modem_type: Fail to set the profile."
			exit 0
		fi
	else
		echo "$modem_type: skip to set the ISP profile."
	fi

	# set COPS.
	is_Docomo=`_is_Docomo_modem`
	if [ "$Dev3G" == "Docomo_dongles" ] || [ "$is_Docomo" -eq "1" ]; then
		echo "COPS: Docomo dongles cannot COPS=0, so skip it."
	elif [ "$modem_vid" == "6797" -a "$modem_pid" == "4098" ]; then # BandLuxe C120.
		echo "COPS: BandLuxe C120 start with CFUN=0, so don't need to unregister the network."
	elif [ "$modem_vid" == "4817" -a "$modem_pid" == "5382" ]; then # Huawei E3276.
		echo "COPS: Huawei E3276 cannot COPS=2/COPS=?, so skip it."
	elif [ "$modem_vid" == "4204" -a "$modem_pid" == "14104" ]; then # Pantech UML290VW: VID=0x106c, PID=0x3718
		echo "COPS: Pantech UML290VW cannot COPS?, so skip it."
	else
		at_ret=`/usr/sbin/modem_at.sh '+COPS?' 2>&1`
		ret=`echo -n "$at_ret" |grep "OK" 2>/dev/null`
		if [ -n "$ret" ]; then
			echo "COPS: Can execute +COPS..."

			ret=`echo -n "$at_ret" |grep "+COPS:" |awk '{print $2}' |awk 'BEGIN{FS=","}{print $1}' 2>/dev/null`
			echo "COPS: original COPS=\"$ret\"."
			if [ "$ret" != "2" ]; then # ret was some strings sometimes so couldn't use '-ne'.
				echo "COPS: set +COPS=2."
				wait_time1=`expr $wandog_interval + $wandog_interval`
				wait_time=`expr $wait_time1 + $modem_reg_time`
				nvram set freeze_duck=$wait_time
				/usr/sbin/modem_at.sh '+COPS=2' "$modem_reg_time"
				if [ -z "$atcmd" ] || [ "$atcmd" != "1" ]; then
					/usr/sbin/modem_at.sh '' # clean the output of +COPS=2.
				fi
				#at_ret=`/usr/sbin/modem_at.sh '+COPS?' 2>&1`
				#ret=`echo -n "$at_ret" |grep "+COPS:" |awk '{print $2}' |awk 'BEGIN{FS=","}{print $1}' 2>/dev/null`
				#echo "COPS: 1. Got ret=$ret."
				#if [ "$ret" != "2" ]; then # ret was some strings sometimes so couldn't use '-ne'.
				#	echo "Can't deregister from network."
				#	exit 6
				#fi
			fi

			# Home service.
			if [ "$modem_roaming" == "0" ]; then
				echo "COPS: set +COPS=0."
				wait_time1=`expr $wandog_interval + $wandog_interval`
				wait_time=`expr $wait_time1 + $modem_reg_time`
				nvram set freeze_duck=$wait_time
				/usr/sbin/modem_at.sh '+COPS=0' "$modem_reg_time"
				#at_ret=`/usr/sbin/modem_at.sh '+COPS?' 2>&1`
				#ret=`echo -n "$at_ret" |grep "+COPS:" |awk '{print $2}' |awk 'BEGIN{FS=","}{print $1}' 2>/dev/null`
				#echo "COPS: 2. Got ret=$ret."
				#if [ "$ret" != "0" ]; then # ret was some strings sometimes so couldn't use '-ne'.
				#	echo "COPS: Can't register to network."
				#	exit 6
				#fi
			elif [ "$modem_roaming_mode" == "1" ] && [ -n "$modem_roaming_isp" ]; then
				# roaming manually...
				echo "set the roaming station..."
				/usr/sbin/modem_status.sh station "$modem_roaming_isp"
			else
				# roaming automatically...
				echo "roaming automatically..."
				wait_time1=`expr $wandog_interval + $wandog_interval`
				wait_time=`expr $wait_time1 + $modem_reg_time`
				nvram set freeze_duck=$wait_time
				/usr/sbin/modem_at.sh '+COPS=0' "$modem_reg_time"
			fi
		else # the result from CDMA2000 can be "COMMAND NOT SUPPORT", "ERROR".
			echo "COPS: Don't support +COPS."
		fi
	fi

	if [ "$modem_enable" == "1" ]; then
		/usr/sbin/modem_status.sh setmode $modem_mode
	fi

	if [ "$modem_type" != "qmi" -a "$modem_type" != "gobi"  ]; then
		# check the register state after set COPS.
		if [ "$Dev3G" == "Docomo_dongles" ] || [ "$is_Docomo" -eq "1" ]; then
			echo "COPS: Docomo dongles cannot CGATT=1, so skip it."
		elif [ "$modem_vid" == "4204" -a "$modem_pid" == "14104" ]; then # Pantech UML290VW: VID=0x106c, PID=0x3718
			echo "COPS: Pantech UML290VW cannot CGATT?, so skip it."
		else
			echo "CGATT: Check the register state..."
			at_ret=`/usr/sbin/modem_at.sh '+CGATT?' 2>&1`
			ret=`echo -n "$at_ret" |grep "OK" 2>/dev/null`
			if [ -n "$ret" ]; then
				tries=1
				ret=`echo -n "$at_ret" |grep "+CGATT: 1" 2>/dev/null`
				while [ $tries -le 10 ] && [ -z "$ret" ]; do
					echo "CGATT: wait for network registered...$tries"
					sleep 1

					at_ret=`/usr/sbin/modem_at.sh '+CGATT?' 2>&1`
					ret=`echo -n "$at_ret" |grep "+CGATT: 1" 2>/dev/null`
					tries=`expr $tries + 1`
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
	fi

	if [ "$modem_vid" == "8193" ];then
		# just for D-Link DWM-156 A7.
		#at_ret=`/usr/sbin/modem_at.sh '+BMHPLMN' 2>&1`
		#ret=`echo -n "$at_ret" |grep "+BMHPLMN: " 2>/dev/null`
		#if [ -n "$ret" ]; then
		#	plmn=`echo $at_ret | awk '{print $2}'`
		#	echo "IMSI: $plmn."
		#fi

		# put the dongle in the general procedure.
		exit 0

		at_ret=`/usr/sbin/modem_at.sh '+CFUN=1' 2>&1`
		ret=`echo -n $at_ret |grep "OK" 2>/dev/null`
		if [ -z "$ret" ]; then
			echo "Fail to set CFUN=1."
			exit -1
		fi

		tries=1
		at_ret=`/usr/sbin/modem_at.sh '+CGATT?' 2>&1`
		ret=`echo -n $at_ret |grep "+CGATT: 1" 2>/dev/null`
		while [ $tries -le 10 ] && [ -z "$ret" ]; do
			echo "wait for network registered...$tries"
			sleep 1

			at_ret=`/usr/sbin/modem_at.sh '+CGATT?' 2>&1`
			ret=`echo -n $at_ret |grep "+CGATT: 1" 2>/dev/null`
			tries=`expr $tries + 1`
		done

		if [ "$at_ret" == "" ]; then
			echo "Fail to register network, please check."
			exit -1
		fi

		echo "Successfull to register network."
	elif [ "$modem_type" == "gobi" ]; then
		echo "Connect the line automatically."
		if [ "$usb_gobi2" == "1" ]; then
			at_ret=`/usr/sbin/modem_at.sh '+CAUTOCONNECT=1' 2>&1`
			ret=`echo -n $at_ret |grep "OK" 2>/dev/null`
			if [ -z "$ret" ]; then
				echo "Gobi2: Fail to start the autoconnect."
				exit 0
			fi
		else
			# WDS set the autoconnect & roaming
			# autoconnect: 0, disable; 1, enable; 2, pause.
			# roaming: 0, allow; 1, disable. Only be activated when autoconnect is enabled.
			if [ "$modem_roaming" == "0" ]; then
				echo "Disable roaming..."
				gobi_api $qcqmi SetEnhancedAutoconnect 1 1
			else
				if [ "$modem_roaming_mode" == "1" ] && [ -n "$modem_roaming_isp" ]; then
					echo "Start the manual roaming ..."
				else
					echo "Start the automatic roaming ..."
				fi
				gobi_api $qcqmi SetEnhancedAutoconnect 1 0
			fi
		fi

		at_ret=`/usr/sbin/modem_at.sh '+CGNWS' 2>&1`
		at_cgnws=`echo -n $at_ret |grep "+CGNWS:" |awk 'BEGIN{FS=":"}{print $2}' 2>/dev/null`
		if [ -n "$at_cgnws" ]; then
			mcc=`echo -n "$at_cgnws" |awk 'BEGIN{FS=","}{print $5}' 2>/dev/null`
			mnc=`echo -n "$at_cgnws" |awk 'BEGIN{FS=","}{print $6}' 2>/dev/null`
			target=$mcc$mnc
			len=${#target}
			target=`echo -n $modem_imsi |cut -c '1-'$len 2>/dev/null`

			if [ "$mcc$mnc" == "$target" ]; then
				spn=`echo -n "$at_cgnws" |awk 'BEGIN{FS=","}{print $7}' 2>/dev/null`
				if [ "$modem_spn" == "" -a "$spn" != "" -a "$spn" != "NULL" ]; then
					nvram set modem_spn=$spn
				fi

				# useless temparily.
				#isp=`echo -n "$at_cgnws" |awk 'BEGIN{FS=","}{print $8}' 2>/dev/null` # ISP long name
				#if [ "$isp" != "" -a "$isp" != "NULL" ]; then
				#	nvram set modem_isp=$isp
				#else
				#	isp=`echo -n "$at_cgnws" |awk 'BEGIN{FS=","}{print $9}' 2>/dev/null` # ISP short name
				#	if [ "$isp" != "" -a "$isp" != "NULL" ]; then
				#		nvram set modem_isp=$isp
				#	fi
				#fi
			fi
		fi
	fi

	echo "modem_enable($modem_dev): done."
fi

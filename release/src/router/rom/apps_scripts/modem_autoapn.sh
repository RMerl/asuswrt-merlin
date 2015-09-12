#!/bin/sh
# $1: output, $2: special IMSI.
# echo "This is a script to capture the settings of SIM."


modem_type=`nvram get usb_modem_act_type`
act_node1="usb_modem_act_int"
act_node2="usb_modem_act_bulk"
modem_vid=`nvram get usb_modem_act_vid`
modem_autoapn=`nvram get modem_autoapn`
modem_imsi=
apps_local_space=`nvram get apps_local_space`
dataf="$apps_local_space/spn_asus.dat"
#modem_prefix="modem_"
modem_prefix="test_modem_"


act_node=
#if [ "$modem_type" = "tty" -o "$modem_type" = "mbim" ]; then
#	if [ "$modem_type" = "tty" -a "$modem_vid" = "6610" ]; then # e.q. ZTE MF637U
#		act_node=$act_node1
#	else
#		act_node=$act_node2
#	fi
#else
	act_node=$act_node1
#fi

modem_act_node=`nvram get $act_node`
if [ -z "$modem_act_node" ]; then
	find_modem_node.sh

	modem_act_node=`nvram get $act_node`
	if [ -z "$modem_act_node" ]; then
		echo "Can't get $act_node!"
		exit 1
	fi
fi

if [ "$1" = "set" ]; then
	modem_imsi=$2
else
	modem_imsi=`nvram get usb_modem_act_imsi |cut -c '1-6' 2>/dev/null`
fi
if [ -z "$modem_imsi" ]; then
	modem_status.sh imsi
	modem_imsi=`nvram get usb_modem_act_imsi |cut -c '1-6' 2>/dev/null`
	if [ -z "$modem_imsi" ]; then
		echo "Can't get IMI of SIM!"
		exit 2
	fi
fi

if [ "$1" = "set" ]; then
	content=`awk '/^'"$modem_imsi"',/ {print $0 "," NR; exit}' $dataf 2>/dev/null`
else
	total_line=`wc -l $dataf |awk '{print $1}' 2>/dev/null`
	nvram set usb_modem_auto_lines=$total_line
	nvram set usb_modem_auto_running=1

	modem_imsi_s=`echo $modem_imsi |cut -c 1-6 2>/dev/null`
	content=`awk '/^'"$modem_imsi_s"',/ {print $0 "," NR; exit}' $dataf 2>/dev/null`
	if [ -z "$content" ]; then
		modem_imsi_s=`echo $modem_imsi |cut -c 1-5 2>/dev/null`
		content=`awk '/^'"$modem_imsi_s"',/ {print $0 "," NR; exit}' $dataf 2>/dev/null`
	fi

	nvram set usb_modem_auto_running=$total_line
fi

if [ -z "$content" ]; then
	nvram set g3err_imsi=1
	echo "Can't get the APN mapping automatically!"
	exit 3
fi

compare=`echo "$content" |awk '{FS=","; print $1}' 2>/dev/null`
modem_isp=`echo "$content" |awk '{FS=","; print $2}' 2>/dev/null`
modem_spn=`echo "$content" |awk '{FS=","; print $7}' 2>/dev/null`
[ -z "$modem_spn" ] && modem_spn="$modem_isp"
modem_apn=`echo "$content" |awk '{FS=","; print $6}' 2>/dev/null`
modem_dial=`echo "$content" |awk '{FS=","; print $3}' 2>/dev/null`
modem_user=`echo "$content" |awk '{FS=","; print $4}' 2>/dev/null`
modem_pass=`echo "$content" |awk '{FS=","; print $5}' 2>/dev/null`
line=`echo "$content" |awk '{FS=","; print $8}' 2>/dev/null`

modem_country_num=`echo $modem_imsi |cut -c 1-3 2>/dev/null`
if [ -z "$modem_country_num" ]; then
	echo "Cant get the country number from IMSI."
elif [ $modem_country_num -eq 505 ]; then
	modem_country=AU
elif [ $modem_country_num -eq 232 ]; then
	modem_country=AT
elif [ $modem_country_num -eq 218 ]; then
	modem_country=BA
elif [ $modem_country_num -eq 724 ]; then
	modem_country=BR
elif [ $modem_country_num -eq 528 ]; then
	modem_country=BN
elif [ $modem_country_num -eq 284 ]; then
	modem_country=BG
elif [ $modem_country_num -eq 302 ]; then
	modem_country=CA
elif [ $modem_country_num -eq 460 ]; then
	modem_country=CN
elif [ $modem_country_num -eq 230 ]; then
	modem_country=CZ
elif [ $modem_country_num -eq 238 ]; then
	modem_country=DK
elif [ $modem_country_num -eq 370 ]; then
	modem_country=DO
elif [ $modem_country_num -eq 602 ]; then
	modem_country=EG
elif [ $modem_country_num -eq 244 ]; then
	modem_country=FI
elif [ $modem_country_num -eq 262 ]; then
	modem_country=DE
elif [ $modem_country_num -eq 454 ]; then
	modem_country=HK
elif [ $modem_country_num -eq 404 ]; then
	modem_country=IN
elif [ $modem_country_num -eq 510 ]; then
	modem_country=ID
elif [ $modem_country_num -eq 222 ]; then
	modem_country=IT
elif [ $modem_country_num -eq 440 ]; then
	modem_country=JP
elif [ $modem_country_num -eq 502 ]; then
	modem_country=MY
elif [ $modem_country_num -eq 204 ]; then
	modem_country=NL
elif [ $modem_country_num -eq 530 ]; then
	modem_country=NZ
elif [ $modem_country_num -eq 242 ]; then
	modem_country=NO
elif [ $modem_country_num -eq 515 ]; then
	modem_country=PH
elif [ $modem_country_num -eq 260 ]; then
	modem_country=PL
elif [ $modem_country_num -eq 268 ]; then
	modem_country=PT
elif [ $modem_country_num -eq 226 ]; then
	modem_country=RO
elif [ $modem_country_num -eq 250 ]; then
	modem_country=RU
elif [ $modem_country_num -eq 525 ]; then
	modem_country=SG
elif [ $modem_country_num -eq 231 ]; then
	modem_country=SK
elif [ $modem_country_num -eq 655 ]; then
	modem_country=ZA
elif [ $modem_country_num -eq 214 ]; then
	modem_country=ES
elif [ $modem_country_num -eq 240 ]; then
	modem_country=SE
elif [ $modem_country_num -eq 466 ]; then
	modem_country=TW
elif [ $modem_country_num -eq 520 ]; then
	modem_country=TH
elif [ $modem_country_num -eq 286 ]; then
	modem_country=TR
elif [ $modem_country_num -eq 255 ]; then
	modem_country=UA
elif [ $modem_country_num -eq 234 ]; then
	modem_country=UK
elif [ $modem_country_num -eq 310 ]; then
	modem_country=US
elif [ $modem_country_num -eq 452 ]; then
	modem_country=VN
elif [ $modem_country_num -eq 425 ]; then
	modem_country=IL
fi

if [ "$1" = "console" ]; then
	echo "   line: $line."
	echo "   imsi: $modem_imsi."
	echo "country: $modem_country."
	echo "    isp: $modem_isp."
	echo "    apn: $modem_apn."
	echo "    spn: $modem_spn."
	echo "   dial: $modem_dial."
	echo "   user: $modem_user."
	echo "   pass: $modem_pass."
elif [ "$1" = "set" ]; then
	nvram set usb_modem_auto_imsi="$modem_imsi"
	nvram set modem_country="$modem_country"
	modem_isp=`nvram get modem_roaming_isp`
	nvram set modem_isp="$modem_isp"
	nvram set modem_spn="$modem_spn"
	nvram set modem_apn="$modem_apn"
	nvram set modem_dialnum="$modem_dial"
	nvram set modem_user="$modem_user"
	nvram set modem_pass="$modem_pass"
else
	nvram set usb_modem_auto_imsi="$compare"
	nvram set usb_modem_auto_country="$modem_country"
	nvram set usb_modem_auto_isp="$modem_isp"
	nvram set usb_modem_auto_spn="$modem_spn"
	nvram set usb_modem_auto_apn="$modem_apn"
	nvram set usb_modem_auto_dialnum="$modem_dial"
	nvram set usb_modem_auto_user="$modem_user"
	nvram set usb_modem_auto_pass="$modem_pass"

	if [ "$modem_autoapn" = "1" ]; then
		nvram set modem_country="$modem_country"
		nvram set modem_isp="$modem_isp"
		nvram set modem_spn="$modem_spn"
		nvram set modem_apn="$modem_apn"
		nvram set modem_dialnum="$modem_dial"
		nvram set modem_user="$modem_user"
		nvram set modem_pass="$modem_pass"
	fi
fi

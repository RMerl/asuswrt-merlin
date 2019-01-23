#!/bin/sh

wget_timeout=`nvram get apps_wget_timeout`
wget_options="-q -t 2 -T $wget_timeout --no-check-certificate"

nvram set sig_state_upgrade=0 # INITIALIZING
nvram set sig_state_error=0

#openssl support rsa check
rsa_enabled=`nvram show | grep rc_support | grep HTTPS`

sig_file=`nvram get SKU`_`nvram get sig_state_info`_un.zip
if [ "$rsa_enabled" != "" ]; then
sig_rsasign=`nvram get SKU`_`nvram get sig_state_info`_rsa.zip
fi

# get signature zip file
forsq=`nvram get apps_sq`
echo 3 > /proc/sys/vm/drop_caches
if [ "$forsq" == "1" ]; then
	echo "---- wget fw sq ----" > /tmp/sig_upgrade.log
	wget $wget_options https://dlcdnets.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/$sig_file -O /tmp/rule.trf
	if [ "$rsa_enabled" != "" ]; then
		wget $wget_options https://dlcdnets.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/$sig_rsasign -O /tmp/rsasign.bin
	fi
else
	echo "---- wget fw Real ----" > /tmp/webs_upgrade.log
	wget $wget_options https://dlcdnets.asus.com/pub/ASUS/wireless/ASUSWRT/$sig_file -O /tmp/rule.trf
	if [ "$rsa_enabled" != "" ]; then
		wget $wget_options https://dlcdnets.asus.com/pub/ASUS/wireless/ASUSWRT/$sig_rsasign -O /tmp/rsasign.bin
	fi
fi	

if [ "$?" != "0" ]; then	#download failure
	nvram set sig_state_error=1
else
	nvram set sig_state_upgrade=2	
	echo "---- mv trf OK ----" >> /tmp/sig_upgrade.log
	if [ "$rsa_enabled" != "" ]; then
		nvram set bwdpi_rsa_check=0
		rsasign_sig_check /tmp/rule.trf
		sleep 1
	fi

	if [ "$rsa_enabled" != "" ]; then
		rsasign_check_ret=`nvram get bwdpi_rsa_check`
	fi

	if [ "$rsasign_check_ret" == "1" ]; then
		echo "---- sig check OK ----" >> /tmp/sig_upgrade.log
		if [ -f /jffs/signature/rule.trf ];then
			echo "---- sig rule mv /tmp to /jffs/signature ----" >> /tmp/sig_upgrade.log
			rm /jffs/signature/rule.trf
			mv /tmp/rule.trf /jffs/signature/rule.trf
		else
			echo "---- sig rule mv jffs ----" >> /tmp/sig_upgrade.log
			mkdir /jffs/signature
			mv /tmp/rule.trf /jffs/signature/rule.trf
		fi
		if [ "$1" == "" ];then
			rc rc_service restart_wrs
			nvram set sig_update_t=`date +%s`	#set timestamp for download signature and restart_wrs
		else
			echo "do nothing..."	
		fi
	else
		echo "---- sig check error ----" >> /tmp/sig_upgrade.log
		nvram set sig_state_error=3	# wrong sig trf
	fi
fi

rm -f /tmp/rsasign.bin

nvram set sig_state_upgrade=1

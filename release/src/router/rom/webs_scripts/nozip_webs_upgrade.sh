#!/bin/sh

wget_timeout=`nvram get apps_wget_timeout`
#wget_options="-nv -t 2 -T $wget_timeout --dns-timeout=120"
wget_options="-q -t 2 -T $wget_timeout"

nvram set webs_state_upgrade=0 # INITIALIZING
nvram set webs_state_error=0

#openssl support rsa check
rsa_enabled=`nvram show | grep rc_support | grep HTTPS`

touch /tmp/update_url
update_url=`cat /tmp/update_url`
#update_url="http://192.168.123.198"

firmware_file=`nvram get productid`_`nvram get webs_state_info`_un.zip
if [ "$rsa_enabled" != "" ]; then
firmware_rsasign=`nvram get productid`_`nvram get webs_state_info`_rsa.zip
fi

# get firmware zip file
forsq=`nvram get apps_sq`
urlpath=`nvram get webs_state_url`
echo 3 > /proc/sys/vm/drop_caches
if [ "$update_url" != "" ]; then
	echo "---- wget fw nvram webs_state_url ----" > /tmp/webs_upgrade.log
	wget $wget_options ${update_url}/$firmware_file -O /tmp/linux.trx
	if [ "$rsa_enabled" != "" ]; then
		wget $wget_options ${update_url}/$firmware_rsasign -O /tmp/rsasign.bin
	fi
elif [ "$forsq" == "1" ]; then
	echo "---- wget fw sq ----" > /tmp/webs_upgrade.log
	wget $wget_options http://dlcdnet.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/$firmware_file -O /tmp/linux.trx
	if [ "$rsa_enabled" != "" ]; then
		wget $wget_options http://dlcdnet.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/$firmware_rsasign -O /tmp/rsasign.bin
	fi
elif [ "$urlpath" == "" ]; then
	echo "---- wget fw Real ----" > /tmp/webs_upgrade.log
	wget $wget_options http://dlcdnet.asus.com/pub/ASUS/wireless/ASUSWRT/$firmware_file -O /tmp/linux.trx
	if [ "$rsa_enabled" != "" ]; then
		wget $wget_options http://dlcdnet.asus.com/pub/ASUS/wireless/ASUSWRT/$firmware_rsasign -O /tmp/rsasign.bin
	fi
else
	echo "---- wget fw URL ----" > /tmp/webs_upgrade.log
	wget $wget_options $urlpath/$firmware_file -O /tmp/linux.trx
	if [ "$rsa_enabled" != "" ]; then
		wget $wget_options $urlpath/$firmware_rsasign -O /tmp/rsasign.bin
	fi
fi	

if [ "$?" != "0" ]; then	#download failure
	nvram set webs_state_error=1
else
	nvram set webs_state_upgrade=2	
	echo "---- mv trx OK ----" >> /tmp/webs_upgrade.log
	nvram set firmware_check=0
	firmware_check /tmp/linux.trx
	sleep 1

	if [ "$rsa_enabled" != "" ]; then
		nvram set rsasign_check=0
		rsasign_check /tmp/linux.trx
		sleep 1
	fi

	firmware_check_ret=`nvram get firmware_check`
	if [ "$rsa_enabled" != "" ]; then
		rsasign_check_ret=`nvram get rsasign_check`
	else
		rsasign_check_ret="1"
	fi


	if [ "$firmware_check_ret" == "1" ] && [ "$rsasign_check_ret" == "1" ]; then
		echo "---- fw check OK ----" >> /tmp/webs_upgrade.log
		/sbin/ejusb -1 0
		rc rc_service restart_upgrade
	else
		echo "---- fw check error ----" >> /tmp/webs_upgrade.log
		nvram set webs_state_error=3	# wrong fw
	fi
fi

rm -f /tmp/rsasign.bin

nvram set webs_state_upgrade=1

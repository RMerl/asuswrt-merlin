#!/bin/sh

wget_timeout=`nvram get apps_wget_timeout`
#wget_options="-nv -t 2 -T $wget_timeout --dns-timeout=120"
wget_options="-q -t 2 -T $wget_timeout"

nvram set webs_state_upgrade=0 # INITIALIZING
nvram set webs_state_error=0

touch /tmp/update_url
update_url=`cat /tmp/update_url`
#update_url="http://192.168.123.198"

firmware_file=`nvram get productid`_`nvram get webs_state_info`_un.zip

# get firmware zip file
forsq=`nvram get apps_sq`
urlpath=`nvram get webs_state_url`
echo 3 > /proc/sys/vm/drop_caches
if [ "$update_url" != "" ]; then
	wget $wget_options ${update_url}/$firmware_file -O /tmp/linux.trx
elif [ "$forsq" == "1" ]; then
	echo "---- wget fw sq ----" >> /tmp/webs_upgrade.log
	wget $wget_options http://dlcdnet.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/$firmware_file -O /tmp/linux.trx
elif [ "$urlpath" == "" ]; then
	echo "---- wget fw Real ----" >> /tmp/webs_upgrade.log
	wget $wget_options http://dlcdnet.asus.com/pub/ASUS/wireless/ASUSWRT/$firmware_file -O /tmp/linux.trx
else
	echo "---- wget fw URL ----" >> /tmp/webs_upgrade.log
	wget $wget_options $urlpath/$firmware_file -O /tmp/linux.trx
fi	

if [ "$?" != "0" ]; then	#download failure
	nvram set webs_state_error=1
else
	nvram set webs_state_upgrade=2	
	echo "---- mv trx OK ----" >> /tmp/webs_upgrade.log
	nvram set firmware_check=0
	firmware_check /tmp/linux.trx
	sleep 1
	firmware_check_ret=`nvram get firmware_check`
	if [ "$firmware_check_ret" == "1" ]; then
		echo "---- fw check OK ----" >> /tmp/webs_upgrade.log
		/sbin/ejusb -1 0
		rc rc_service restart_upgrade
	else
		echo "---- fw check error ----" >> /tmp/webs_upgrade.log
		nvram set webs_state_error=3	# wrong fw
	fi
fi

nvram set webs_state_upgrade=1

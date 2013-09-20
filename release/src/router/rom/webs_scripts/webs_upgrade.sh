#!/bin/sh

wget_timeout=`nvram get apps_wget_timeout`
#wget_options="-nv -t 2 -T $wget_timeout --dns-timeout=120"
wget_options="-q -t 2 -T $wget_timeout"

nvram set webs_state_upgrade=0 # INITIALIZING
nvram set webs_state_error=0

model=`nvram get productid`
firmware_file=`nvram get productid`_`nvram get webs_state_info`_un.zip

# get firmware zip file
forsq=`nvram get apps_sq`
echo 3 > /proc/sys/vm/drop_caches
if [ "$forsq" == "1" ]; then
	echo "---- upgrade sq ----" >> /tmp/webs_upgrade.log
	wget $wget_options http://dlcdnet.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/$firmware_file -O /tmp/linux.zip
else
	wget $wget_options http://dlcdnet.asus.com/pub/ASUS/wireless/ASUSWRT/$firmware_file -O /tmp/linux.zip
fi	

if [ "$?" != "0" ]; then	#download failure
	nvram set webs_state_error=1
else
	nvram set webs_state_upgrade=2
	mv /tmp/linux.zip /tmp/linux.trx
	echo "---- mv trx OK ----" >> /tmp/webs_upgrade.log
	nvram set firmware_check=0
	firmware_check /tmp/linux.trx
	sleep 1
	firmware_check_ret=`nvram get firmware_check`
	if [ "$firmware_check_ret" == "1" ]; then
		echo "---- fw check OK ----" >> /tmp/webs_upgrade.log
		rc rc_service start_upgrade
	else
		echo "---- fw check error ----" >> /tmp/webs_upgrade.log
		nvram set webs_state_error=3	# wrong fw
	fi
fi

nvram set webs_state_upgrade=1

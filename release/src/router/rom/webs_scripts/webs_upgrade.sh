#!/bin/sh

wget_timeout=`nvram get apps_wget_timeout`
#wget_options="-nv -t 2 -T $wget_timeout --dns-timeout=120"
wget_options="-q -t 2 -T $wget_timeout"

nvram set webs_state_upgrade=0 # INITIALIZING
nvram set webs_state_error=0

model=`nvram get productid`
firmware_file=`nvram get productid`_`nvram get webs_state_info`.zip

# get firmware information
wget $wget_options http://dlcdnet.asus.com/pub/ASUS/wireless/ASUSWRT/$firmware_file -O /tmp/linux.zip

if [ "$?" != "0" ]; then
	nvram set webs_state_error=1
else
	# TODO write to flash
	unzip -o /tmp/linux.zip -d /tmp
	rm -f /tmp/linux.zip
	nvram set webs_rm_zip=1
	rc rc_service start_upgrade
fi

nvram set webs_state_upgrade=1




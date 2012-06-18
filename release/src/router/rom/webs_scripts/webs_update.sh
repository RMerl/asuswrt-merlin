#!/bin/sh

wget_timeout=`nvram get apps_wget_timeout`
#wget_options="-nv -t 2 -T $wget_timeout --dns-timeout=120"
wget_options="-q -t 2 -T $wget_timeout"

nvram set webs_state_update=0 # INITIALIZING
nvram set webs_state_error=0

# get firmware information
model=`nvram get productid`
wget $wget_options http://dlcdnet.asus.com/pub/ASUS/LiveUpdate/Release/Wireless/wlan_update.zip -O /tmp/wlan_update.zip

if [ "$?" != "0" ]; then
	nvram set webs_state_error=1
else
	# TODO get and parse information
	unzip -o /tmp/wlan_update.zip -d /tmp 
	firmver=`grep $model /tmp/wlan_update.txt | sed s/.*#FW//;`
	firmver=`echo $firmver | sed s/#.*//;`
	buildno=`echo $firmver | sed s/....//;`
	firmver=`echo $firmver | sed s/$buildno$//;`
	nvram set webs_state_info=${firmver}_${buildno}
	rm -f /tmp/wlan_update.*
fi
nvram set webs_state_update=1

#!/bin/sh

wget_timeout=`nvram get apps_wget_timeout`
#wget_options="-nv -t 2 -T $wget_timeout --dns-timeout=120"
wget_options="-q -t 2 -T $wget_timeout"

nvram set webs_state_update=0 # INITIALIZING
nvram set webs_state_error=0
nvram set webs_state_url=""

# get firmware information
forsq=`nvram get apps_sq`
model=`nvram get productid`
swisscom=`nvram show | grep rc_support | grep swisscom`
if [ "$forsq" == "1" ]; then
	if [ "$swisscom" == "" ]; then
		echo "---- update sq normal----" >> /tmp/webs_upgrade.log
		wget $wget_options http://dlcdnet.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/wlan_update_v2.zip -O /tmp/wlan_update.txt
	else
		echo "---- update sq swisscom----" >> /tmp/webs_upgrade.log
		wget $wget_options http://dlcdnet.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/wlan_update_swisscom.zip -O /tmp/wlan_update.txt		
	fi
else
	if [ "$swisscom" == "" ]; then
		echo "---- update real normal----" >> /tmp/webs_upgrade.log
		wget $wget_options http://dlcdnet.asus.com/pub/ASUS/LiveUpdate/Release/Wireless/wlan_update_v2.zip -O /tmp/wlan_update.txt
	else
		echo "---- update real swisscom----" >> /tmp/webs_upgrade.log
		wget $wget_options http://dlcdnet.asus.com/pub/ASUS/LiveUpdate/Release/Wireless/wlan_update_swisscom.zip -O /tmp/wlan_update.txt
	fi
fi	

if [ "$?" != "0" ]; then
	nvram set webs_state_error=1
else
	# TODO get and parse information
	firmver=`grep $model /tmp/wlan_update.txt | sed s/.*#FW//;`
	firmver=`echo $firmver | sed s/#.*//;`
	buildno=`echo $firmver | sed s/....//;`
	firmver=`echo $firmver | sed s/$buildno$//;`
	extendno=`grep $model /tmp/wlan_update.txt | sed s/.*#EXT//;`
	extendno=`echo $extendno | sed s/#.*//;`
	nvram set webs_state_info=${firmver}_${buildno}_${extendno}
	urlpath=`grep $model /tmp/wlan_update.txt | sed s/.*#URL//;`	
	urlpath=`echo $urlpath | sed s/#.*//;`	
	nvram set webs_state_url=${urlpath}
	rm -f /tmp/wlan_update.*
fi
nvram set webs_state_update=1

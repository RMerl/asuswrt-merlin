#!/bin/sh

wget_timeout=`nvram get apps_wget_timeout`
wget_options="-q -t 2 -T $wget_timeout --no-check-certificate"

nvram set sig_state_update=0 # INITIALIZING
nvram set sig_state_flag=0   # 0: Don't do upgrade  1: Do upgrade	
nvram set sig_state_error=0
#nvram set sig_state_url=""

# current signature information
current_sig_ver=`nvram get bwdpi_sig_ver`
echo $current_sig_ver
current_sig_ver=`echo $current_sig_ver | sed s/'\.'//g;`
echo $current_sig_ver


# get signature information
forsq=`nvram get apps_sq`

wl0_support=`nvram show | grep rc_support | grep 2.4G`

if [ "$wl0_support" != "" ]; then
	country_code=`nvram get wl0_country_code`
else
	country_code=`nvram get wl1_country_code`
fi

model=`nvram get productid`
if [ "$forsq" == "1" ]; then
	echo "---- sig update sq normal----" > /tmp/sig_upgrade.log
	wget $wget_options https://dlcdnets.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/sig_update.zip -O /tmp/sig_update.txt		
else
	echo "---- sig update real normal----" > /tmp/sig_upgrade.log
	wget $wget_options https://dlcdnets.asus.com/pub/ASUS/LiveUpdate/Release/Wireless/sig_update.zip -O /tmp/sig_update.txt
fi	

if [ "$?" != "0" ]; then
	nvram set sig_state_error=1
else
	# TODO get and parse information
	sig_ver=`grep $country_code /tmp/sig_update.txt | sed s/.*#//;`
	echo $sig_ver
	if [ "$sig_ver" == "" ]; then
		sig_ver=`grep WW /tmp/sig_update.txt | sed s/.*#//;`
		nvram set SKU="WW";
	else
		nvram set SKU=$country_code;
	fi
	sig_ver=`echo $sig_ver | sed s/'\.'//g;`
	echo $sig_ver
	nvram set sig_state_info=${sig_ver}
	#urlpath=`grep $model /tmp/sig_update.txt | sed s/.*#URL//;`
	#urlpath=`echo $urlpath | sed s/#.*//;`
	#echo $urlpath
	#nvram set sig_state_url=${urlpath}
	rm -f /tmp/sig_update.*
fi

echo "---- $sig_ver ---" >> /tmp/sig_upgrade.log
if [ "$sig_ver" == "" ]; then
	nvram set sig_state_error=1	# exist no Info
else
	if [ "$current_sig_ver" -lt "$sig_ver" ]; then
		echo "---- sig_ver: $sig_ver ----" >> /tmp/sig_upgrade.log
		nvram set sig_state_flag=1	# Do upgrade
	fi	
fi

nvram set sig_state_update=1

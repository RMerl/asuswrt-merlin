#!/bin/sh

wget_timeout=`nvram get apps_wget_timeout`
#wget_options="-nv -t 2 -T $wget_timeout --dns-timeout=120"
wget_options="-q -t 2 -T $wget_timeout"

fwsite=`nvram get firmware_server`
if [ "$fwsite" == "" ]; then
	nvram set webs_state_error=1
	nvram set webs_state_update=1
	exit
fi

nvram set webs_state_update=0 # INITIALIZING
nvram set webs_state_flag=0   # 0: Don't do upgrade  1: Do upgrade
nvram set webs_state_error=0
nvram set webs_state_url=""

#openssl support rsa check
IS_SUPPORT_NOTIFICATION_CENTER=`nvram get rc_support|grep -i nt_center`
if [ "$IS_SUPPORT_NOTIFICATION_CENTER" != "" ]; then
. /tmp/nc/event.conf
fi
# current firmware information
current_firm=`nvram get firmver`
current_firm=`echo $current_firm | sed s/'\.'//g;`
current_buildno=`nvram get buildno`
current_buildno=`echo $current_buildno | sed s/'\.'//g;`
current_extendno=`nvram get extendno`

#if echo $current_extendno | grep -q -E "(beta|alpha)"; then
#	current_firm_is_beta=1
#else
#	current_firm_is_beta=0
#fi

# Overload extendno: alpha is 11-19, beta is 51-59, release is 100-109.
current_extendno=`echo $current_extendno | sed s/-g.*//;`
current_extendno=`echo $current_extendno | sed "s/^[0-9]$/10&/;"`
current_extendno=`echo $current_extendno | sed s/^alpha/1/;`
current_extendno=`echo $current_extendno | sed s/^beta/5/;`

# get firmware information
forsq=`nvram get apps_sq`
model=`nvram get productid`
model="$model#"

if [ "$forsq" == "1" ]; then
	echo "---- update sq normal----" > /tmp/webs_upgrade.log
	/usr/sbin/wget $wget_options $fwsite/test/wlan_update_v2.zip -O /tmp/wlan_update.txt
else
	echo "---- update real normal----" > /tmp/webs_upgrade.log
	/usr/sbin/wget $wget_options $fwsite/wlan_update_v2.zip -O /tmp/wlan_update.txt
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
	lextendno=`echo $extendno | sed s/-g.*//;`
 	lextendno=`echo $lextendno | sed "s/^[0-9]$/10&/;"`
 	lextendno=`echo $lextendno | sed s/^alpha/1/;`
 	lextendno=`echo $lextendno | sed s/^beta/5/;`
 	nvram set webs_state_info=${firmver}_${buildno}_${extendno}
	urlpath=`grep $model /tmp/wlan_update.txt | sed s/.*#URL//;`
	urlpath=`echo $urlpath | sed s/#.*//;`
	nvram set webs_state_url=${urlpath}

	firmver_beta=`grep $model /tmp/wlan_update.txt | sed s/.*#BETAFW//;`
	firmver_beta=`echo $firmver_beta | sed s/#.*//;`
	buildno_beta=`echo $firmver_beta | sed s/....//;`
	firmver_beta=`echo $firmver_beta | sed s/$buildno_beta$//;`
	extendno_beta=`grep $model /tmp/wlan_update.txt | sed s/.*#BETAEXT//;`
	extendno_beta=`echo $extendno_beta | sed s/#.*//;`
	lextendno_beta=`echo $extendno_beta | sed s/-g.*//;`
 	lextendno_beta=`echo $lextendno_beta | sed "s/^[0-9]$/10&/;"`
 	lextendno_beta=`echo $lextendno_beta | sed s/^alpha/1/;`
 	lextendno_beta=`echo $lextendno_beta | sed s/^beta/5/;`
 	nvram set webs_state_info_beta=${firmver_beta}_${buildno_beta}_${extendno_beta}
	rm -f /tmp/wlan_update.*
fi

echo "---- Have $current_firm $current_buildno $current_extendno----" >> /tmp/webs_upgrade.log
echo "---- Stable available $firmver $buildno $extendno----" >> /tmp/webs_upgrade.log
echo "---- Beta available $firmver_beta $buildno_beta $extendno_beta----" >> /tmp/webs_upgrade.log

update_webs_state_info=`nvram get webs_state_info`
last_webs_state_info=`nvram get webs_last_info` 
if [ "$firmver" == "" ] || [ "$buildno" == "" ] || [ "$lextendno" == "" ]; then
	nvram set webs_state_error=1	# exist no Info
else
	if [ "$current_buildno" -lt "$buildno" ]; then
		echo "---- buildno: $buildno ----" >> /tmp/webs_upgrade.log
		nvram set webs_state_flag=1	# Do upgrade
		if [ "$IS_SUPPORT_NOTIFICATION_CENTER" != "" ]; then
			if [ "$last_webs_state_info" != "$update_webs_state_info" ]; then
				#if [ "$current_firm_is_beta" != 1 ]; then
					Notify_Event2NC "$SYS_FW_NWE_VERSION_AVAILABLE_EVENT" "New FW Available now."	#Send Event to Notification Center
					nvram set webs_last_info="$update_webs_state_info"
				#fi
			fi
		fi
	elif [ "$current_buildno" -eq "$buildno" ]; then
		if [ "$current_firm" -lt "$firmver" ]; then
				echo "---- firmver: $firmver ----" >> /tmp/webs_upgrade.log
				nvram set webs_state_flag=1	# Do upgrade
				if [ "$IS_SUPPORT_NOTIFICATION_CENTER" != "" ]; then
					if [ "$last_webs_state_info" != "$update_webs_state_info" ]; then
						#if [ "$current_firm_is_beta" != 1 ]; then
							Notify_Event2NC "$SYS_FW_NWE_VERSION_AVAILABLE_EVENT" "New FW Available now."	#Send Event to Notification Center
							nvram set webs_last_info="$update_webs_state_info"
						#fi
					fi
				fi
		elif [ "$current_firm" -eq "$firmver" ]; then
			if [ "$current_extendno" -lt "$lextendno" ]; then
				echo "---- lextendno: $lextendno ----" >> /tmp/webs_upgrade.log
				nvram set webs_state_flag=1	# Do upgrade
				if [ "$IS_SUPPORT_NOTIFICATION_CENTER" != "" ]; then
					if [ "$last_webs_state_info" != "$update_webs_state_info" ]; then
						#if [ "$current_firm_is_beta" != 1 ]; then
							Notify_Event2NC "$SYS_FW_NWE_VERSION_AVAILABLE_EVENT" "New FW Available now."	#Send Event to Notification Center
							nvram set webs_last_info="$update_webs_state_info"
						#fi
					fi
				fi
			fi
		fi
	fi
fi

# download releasee note
get_beta_release=0
if [ "$firmver_beta" != "" ] && [ "$buildno_beta" != "" ] && [ "$lextendno_beta" != "" ]; then
	if [ "$current_buildno" -lt "$buildno_beta" ]; then
		get_beta_release=1
	elif [ "$current_buildno" -eq "$buildno_beta" ]; then
		if [ "$current_firm" -lt "$firmver_beta" ]; then
			get_beta_release=1
	   	elif [ "$current_firm" -eq "$firmver_beta" ]; then
			if [ "$current_extendno" -lt "$lextendno_beta" ]; then
				get_beta_release=1
			fi
		fi
	fi
fi

webs_state_flag=`nvram get webs_state_flag`
get_productid=`nvram get productid`
get_productid=`echo $get_productid | sed s/+/plus/;`	#replace 'plus' to '+' for one time
get_preferred_lang=`nvram get preferred_lang`

if [ "$webs_state_flag" -eq "1" ]; then
	releasenote_file0_US=`nvram get webs_state_info`_note.zip
	releasenote_path0="/tmp/release_note0.txt"
	if [ "$forsq" == "1" ]; then
		echo "---- download SQ release note $fwsite/test/$releasenote_file0 ----" >> /tmp/webs_upgrade.log
		/usr/sbin/wget $wget_options $fwsite/test/$releasenote_file0_US -O $releasenote_path0
		echo "---- $fwsite/test/$releasenote_file0 ----" >> /tmp/webs_upgrade.log
	else
		echo "---- download real release note ----" >> /tmp/webs_upgrade.log
		/usr/sbin/wget $wget_options $fwsite/$releasenote_file0_US -O $releasenote_path0
		echo "---- $fwsite/$releasenote_file0 ----" >> /tmp/webs_upgrade.log
	fi

	if [ "$?" != "0" ]; then
		echo "---- download SQ release note failed ----" >> /tmp/webs_upgrade.log
		nvram set webs_state_error=1
	fi
elif [ "$get_beta_release" == "1" ]; then
	releasenote_file1_US=`nvram get webs_state_info_beta`_note.zip
	releasenote_path1="/tmp/release_note1.txt"
	if [ "$forsq" == "1" ]; then
		echo "---- download SQ beta release note $fwsite/test/$releasenote_file0 ----" >> /tmp/webs_upgrade.log
		/usr/sbin/wget $wget_options $fwsite/test/$releasenote_file1_US -O $releasenote_path1
		echo "---- $fwsite/test/$releasenote_file1 ----" >> /tmp/webs_upgrade.log
	else
		echo "---- download real beta release note ----" >> /tmp/webs_upgrade.log
		/usr/sbin/wget $wget_options $fwsite/$releasenote_file1_US -O $releasenote_path1
		echo "---- $fwsite/$releasenote_file1 ----" >> /tmp/webs_upgrade.log
	fi
	if [ "$?" != "0" ]; then
		echo "---- download SQ release note failed ----" >> /tmp/webs_upgrade.log
		nvram set webs_state_error=1
	fi
fi

nvram set webs_state_update=1

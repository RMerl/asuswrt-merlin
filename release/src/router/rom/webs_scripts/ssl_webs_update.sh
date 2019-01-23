#!/bin/sh

wget_timeout=`nvram get apps_wget_timeout`
#wget_options="-nv -t 2 -T $wget_timeout --dns-timeout=120"
wget_options="-q -t 2 -T $wget_timeout --no-check-certificate"

nvram set webs_state_update=0 # INITIALIZING
nvram set webs_state_flag=0   # 0: Don't do upgrade  1: Do upgrade	
nvram set webs_state_error=0
nvram set webs_state_odm=0
nvram set webs_state_url=""

#openssl support rsa check
IS_SUPPORT_NOTIFICATION_CENTER=`nvram get rc_support|grep -i nt_center`
if [ "$IS_SUPPORT_NOTIFICATION_CENTER" != "" ]; then
. /tmp/nc/event.conf
fi
# current firmware information
current_firm=`nvram get firmver`
current_firm=`echo $current_firm | sed s/'\.'//g;`
current_firm_1st_bit=${current_firm:0:1}
current_buildno=`nvram get buildno`
current_extendno=`nvram get extendno`
current_extendno=`echo $current_extendno | sed s/-g.*//;`

# get firmware information
forsq=`nvram get apps_sq`
model=`nvram get productid`
model="$model#"
odmpid=`nvram get odmpid`
odmpid="$odmpid#"

# change info path
model_31="0"
model_30="0"
if [ "$model" == "RT-N18U#" ]; then
	model_31="1"
elif [ "$model" == "RT-AC68U#" ] || [ "$model" == "RT-AC56S#" ] || [ "$model" == "RT-AC56U#" ]; then
	model_30="1"	#Use another info after middle firmware
fi

if [ "$forsq" == "1" ]; then
	if [ "$model_31" == "1" ]; then
		echo "---- update sq normal for model_31 ----" >> /tmp/webs_upgrade.log
		wget $wget_options https://dlcdnets.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/wlan_update_31.zip -O /tmp/wlan_update.txt
	elif [ "$model_30" == "1" ]; then
		echo "---- update sq normal for model_30 ----" >> /tmp/webs_upgrade.log
		wget $wget_options https://dlcdnets.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/wlan_update_30.zip -O /tmp/wlan_update.txt
	else
		echo "---- update sq normal----" >> /tmp/webs_upgrade.log
		wget $wget_options https://dlcdnets.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/wlan_update_v2.zip -O /tmp/wlan_update.txt		
	fi
else
	if [ "$model_31" == "1" ]; then
		echo "---- update real normal for model_31 ----" >> /tmp/webs_upgrade.log
		wget $wget_options https://dlcdnets.asus.com/pub/ASUS/LiveUpdate/Release/Wireless/wlan_update_31.zip -O /tmp/wlan_update.txt
	elif [ "$model_30" == "1" ]; then
		echo "---- update real normal for model_30 ----" >> /tmp/webs_upgrade.log
		wget $wget_options https://dlcdnets.asus.com/pub/ASUS/LiveUpdate/Release/Wireless/wlan_update_30.zip -O /tmp/wlan_update.txt
	else
		echo "---- update real normal----" >> /tmp/webs_upgrade.log
		wget $wget_options https://dlcdnets.asus.com/pub/ASUS/LiveUpdate/Release/Wireless/wlan_update_v2.zip -O /tmp/wlan_update.txt
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
	lextendno=`echo $extendno | sed s/-g.*//;`
	# webs_state_info for odmpid
	if [ "$odmpid" != "#" ]; then
		firmver_odmpid=`grep $odmpid /tmp/wlan_update.txt | sed s/.*#FW//;`
		firmver_odmpid=`echo $firmver_odmpid | sed s/#.*//;`
		buildno_odmpid=`echo $firmver_odmpid | sed s/....//;`
		firmver_odmpid=`echo $firmver_odmpid | sed s/$buildno_odmpid$//;`
		extendno_odmpid=`grep $odmpid /tmp/wlan_update.txt | sed s/.*#EXT//;`
		extendno_odmpid=`echo $extendno_odmpid | sed s/#.*//;`
		lextendno_odmpid=`echo $extendno_odmpid | sed s/-g.*//;`
	else
		firmver_odmpid=""
		buildno_odmpid=""
		lextendno_odmpid=""
	fi
	echo "---- odmpid : $firmver_odmpid $buildno_odmpid $lextendno_odmpid ----" >> /tmp/webs_upgrade.log
	echo "---- productid : $firmver $buildno $lextendno ----" >> /tmp/webs_upgrade.log
	if [ "$firmver_odmpid" == "" ] || [ "$buildno_odmpid" == "" ] || [ "$lextendno_odmpid" == "" ]; then
		nvram set webs_state_info=${firmver}_${buildno}_${extendno}
	else
		nvram set webs_state_info=${firmver_odmpid}_${buildno_odmpid}_${extendno_odmpid}
		firmver="$firmver_odmpid"
		buildno="$buildno_odmpid"
		lextendno="$lextendno_odmpid"
		nvram set webs_state_odm=1		# with Live Update odmpid sku
	fi	
	urlpath=`grep $model /tmp/wlan_update.txt | sed s/.*#URL//;`
	urlpath=`echo $urlpath | sed s/#.*//;`
	urlpath_odmpid=`grep $odmpid /tmp/wlan_update.txt | sed s/.*#URL//;`
	urlpath_odmpid=`echo $urlpath_odmpid | sed s/#.*//;`
	if [ "$urlpath_odmpid" == "" ]; then
		nvram set webs_state_url=${urlpath}
	else
		nvram set webs_state_url=${urlpath_odmpid}
		urlpath="$urlpath_odmpid"
	fi	
	firmver_beta=`grep $model /tmp/wlan_update.txt | sed s/.*#BETAFW//;`
	firmver_beta=`echo $firmver_beta | sed s/#.*//;`
	buildno_beta=`echo $firmver_beta | sed s/....//;`
	firmver_beta=`echo $firmver_beta | sed s/$buildno_beta$//;`
	extendno_beta=`grep $model /tmp/wlan_update.txt | sed s/.*#BETAEXT//;`
	extendno_beta=`echo $extendno_beta | sed s/#.*//;`
	lextendno_beta=`echo $extendno_beta | sed s/-g.*//;`
	# webs_state_info for odmpid
        if [ "$odmpid" != "#" ]; then
		firmver_beta_odmpid=`grep $odmpid /tmp/wlan_update.txt | sed s/.*#BETAFW//;`
		firmver_beta_odmpid=`echo $firmver_beta_odmpid | sed s/#.*//;`
		buildno_beta_odmpid=`echo $firmver_beta_odmpid | sed s/....//;`
		firmver_beta_odmpid=`echo $firmver_beta_odmpid | sed s/$buildno_beta_odmpid$//;`
		extendno_beta_odmpid=`grep $odmpid /tmp/wlan_update.txt | sed s/.*#BETAEXT//;`
		extendno_beta_odmpid=`echo $extendno_beta_odmpid | sed s/#.*//;`
		lextendno_beta_odmpid=`echo $extendno_beta_odmpid | sed s/-g.*//;`
	else
		firmver_beta_odmpid=""
		buildno_beta_odmpid=""
		lextendno_beta_odmpid=""
	fi
	echo "---- odmpid : $firmver_beta_odmpid $buildno_beta_odmpid $lextendno_beta_odmpid ----" >> /tmp/webs_upgrade.log
	echo "---- productid : $firmver_beta $buildno_beta $lextendno_beta ----" >> /tmp/webs_upgrade.log
	
	odmpid_support=`nvram get webs_state_odm`
	if [ "$odmpid_support" == "1" ]; then
		nvram set webs_state_info_beta=${firmver_beta_odmpid}_${buildno_beta_odmpid}_${extendno_beta_odmpid}
		firmver_beta="$firmver_beta_odmpid"
		buildno_beta="$buildno_beta_odmpid"
		lextendno_beta="$lextendno_beta_odmpid"
	else
		nvram set webs_state_info_beta=${firmver_beta}_${buildno_beta}_${extendno_beta}
	fi	
	rm -f /tmp/wlan_update.*
fi

echo "---- $current_firm $current_buildno $current_extendno----" >> /tmp/webs_upgrade.log
update_webs_state_info=`nvram get webs_state_info`
last_webs_state_info=`nvram get webs_last_info` 
if [ "$firmver" == "" ] || [ "$buildno" == "" ] || [ "$lextendno" == "" ]; then
	nvram set webs_state_error=1	# exist no Info
else
	if [ "$current_buildno" -lt "$buildno" ]; then		
		nvram set webs_state_flag=1	# Do upgrade
		if [ "$IS_SUPPORT_NOTIFICATION_CENTER" != "" ]; then
			if [ "$last_webs_state_info" != "$update_webs_state_info" ]; then
				#if [ "$current_firm_1st_bit" != 9 ]; then
				Notify_Event2NC "$SYS_FW_NWE_VERSION_AVAILABLE_EVENT" "New FW Available now."	#Send Event to Notification Center
				nvram set webs_last_info="$update_webs_state_info"
				#fi	
			fi
		fi
	elif [ "$current_buildno" -eq "$buildno" ]; then
		if [ "$current_firm" -lt "$firmver"]; then 
				echo "---- firmver: $firmver ----" >> /tmp/webs_upgrade.log
				nvram set webs_state_flag=1	# Do upgrade
				if [ "$IS_SUPPORT_NOTIFICATION_CENTER" != "" ]; then
					if [ "$last_webs_state_info" != "$update_webs_state_info" ]; then
						#if [ "$current_firm_1st_bit" != 9 ]; then
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
						#if [ "$current_firm_1st_bit" != 9 ]; then
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
webs_state_flag=`nvram get webs_state_flag`
get_productid=`nvram get productid`
if [ "$odmpid_support" == "1" ]; then
	get_productid=`nvram get odmpid`
fi
get_productid=`echo $get_productid | sed s/+/plus/;`	#replace 'plus' to '+' for one time
get_preferred_lang=`nvram get preferred_lang`
LANG="$get_preferred_lang"

releasenote_file0=`echo $get_productid`_`nvram get webs_state_info`_"$LANG"_note.zip
releasenote_file0_US=`echo $get_productid`_`nvram get webs_state_info`_US_note.zip
releasenote_path0="/tmp/release_note0.txt"
releasenote_file1=`echo $get_productid`_`nvram get webs_state_info_beta`_"$LANG"_note.zip
releasenote_file1_US=`echo $get_productid`_`nvram get webs_state_info_beta`_US_note.zip
releasenote_path1="/tmp/release_note1.txt"

if [ "$webs_state_flag" -eq "1" ]; then
	if [ "$forsq" == "1" ]; then
		echo "---- download SQ release note https://dlcdnets.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/$releasenote_file0 ----" >> /tmp/webs_upgrade.log
		wget $wget_options https://dlcdnets.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/$releasenote_file0 -O $releasenote_path0
		if [ "$?" != "0" ]; then
			wget $wget_options https://dlcdnets.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/$releasenote_file0_US -O $releasenote_path0
		fi
		echo "---- https://dlcdnets.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/$releasenote_file0 ----" >> /tmp/webs_upgrade.log
		wget $wget_options https://dlcdnets.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/$releasenote_file1 -O $releasenote_path1
		if [ "$?" != "0" ]; then
			wget $wget_options https://dlcdnets.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/$releasenote_file1_US -O $releasenote_path1
		fi
		echo "---- https://dlcdnets.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/$releasenote_file1 ----" >> /tmp/webs_upgrade.log
	else
		echo "---- download real release note ----" >> /tmp/webs_upgrade.log
		wget $wget_options https://dlcdnets.asus.com/pub/ASUS/wireless/ASUSWRT/$releasenote_file0 -O $releasenote_path0
		if [ "$?" != "0" ]; then
			wget $wget_options https://dlcdnets.asus.com/pub/ASUS/wireless/ASUSWRT/$releasenote_file0_US -O $releasenote_path0
		fi
		echo "---- https://dlcdnets.asus.com/pub/ASUS/wireless/ASUSWRT/$releasenote_file0 ----" >> /tmp/webs_upgrade.log
		wget $wget_options https://dlcdnets.asus.com/pub/ASUS/wireless/ASUSWRT/$releasenote_file1 -O $releasenote_path1
		if [ "$?" != "0" ]; then
			wget $wget_options https://dlcdnets.asus.com/pub/ASUS/wireless/ASUSWRT/$releasenote_file1_US -O $releasenote_path1
		fi
		echo "---- https://dlcdnets.asus.com/pub/ASUS/wireless/ASUSWRT/$releasenote_file1 ----" >> /tmp/webs_upgrade.log
	fi
	
	if [ "$?" != "0" ]; then
		echo "---- download SQ release note failed ----" >> /tmp/webs_upgrade.log
		nvram set webs_state_error=1		
	fi
fi

nvram set webs_state_update=1

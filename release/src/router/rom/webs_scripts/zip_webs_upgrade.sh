#!/bin/sh

wget_timeout=`nvram get apps_wget_timeout`
#wget_options="-nv -t 2 -T $wget_timeout --dns-timeout=120"
wget_options="-q -t 2 -T $wget_timeout"

nvram set webs_state_upgrade=0 # INITIALIZING
nvram set webs_state_error=0

model=`nvram get productid`
firmware_file=`nvram get productid`_`nvram get webs_state_info`.zip

force_free_ram(){
	wlconf eth1 down
	wlconf eth2 down
	rmmod wl_high
	rmmod wl
	echo 3 > /proc/sys/vm/drop_caches
	sleep 3
	echo "---- end of force_free_ram ----" >> /tmp/webs_upgrade.log
}

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
	echo 3 > /proc/sys/vm/drop_caches
	s1=`ls -l /tmp/linux.zip | awk '{print $5}'`	#fw size
	s2=`df /tmp | grep /tmp | awk '{print $4*1024}'`		# /tmp free space
	echo "---- s1= ----" >> /tmp/webs_upgrade.log
	echo $s1 >> /tmp/webs_upgrade.log	
	echo "---- s2= ----" >> /tmp/webs_upgrade.log
	echo $s2 >> /tmp/webs_upgrade.log
	/sbin/ejusb -1 0
	rc rc_service stop_upgrade
	if expr $s1 \> $s2 ; then		# /tmp free space Not enough
		force_free_ram
		sleep 15
		s21=`df /tmp | grep /tmp | awk '{print $4*1024}'`	# /tmp free space after force_free_ram
		if expr $s1 \> $s21 ; then
			nvram set webs_state_error=2
			echo "---- /tmp free space shortage ----" >> /tmp/webs_upgrade.log
			sleep 3
		else
			mkdir /tmp/mytmpfs
			s3=`df /tmp | grep /tmp | awk '{print $2*1024}'`	# /tmp total size
			echo "---- s3= ----" >> /tmp/webs_upgrade.log
			echo $s3 >> /tmp/webs_upgrade.log
			if expr $s3 \< 16000*1024 ; then	#flash < 16M
				mount -t tmpfs -o size=16M,nr_inodes=10k,mode=700 tmpfs /tmp/mytmpfs
				echo "---- mount tmpfs 16M ----" >> /tmp/webs_upgrade.log
			else	#flash > 16M
				mount -t tmpfs -o size=32M,nr_inodes=10k,mode=700 tmpfs /tmp/mytmpfs
				echo "---- mount tmpfs 32M ----" >> /tmp/webs_upgrade.log
			fi	
			unzip -o /tmp/linux.zip -d /tmp/mytmpfs/
			if [ "$?" != "0" ]; then	#unzip failure
				nvram set webs_state_error=3
				echo "---- f unzip error ----" >> /tmp/webs_upgrade.log
				sleep 3
			else
				nvram set webs_state_upgrade=2
				sleep 10
				echo 3 > /proc/sys/vm/drop_caches
				rm /tmp/linux.zip
				mv /tmp/mytmpfs/*.trx /tmp/linux.trx
				echo "---- /mytmpfs OK ----" >> /tmp/webs_upgrade.log
				sleep 3
				umount /tmp/mytmpfs
			fi
		fi
	else	# tmpfs available memory enough
		unzip -o /tmp/linux.zip -d /tmp/
		if [ "$?" != "0" ]; then	#unzip failure
			nvram set webs_state_error=3
			echo "---- t unzip error ----" >> /tmp/webs_upgrade.log
			sleep 3
		else	#unzip OK
			nvram set webs_state_upgrade=2
			rm -f /tmp/linux.zip
			mv /tmp/*.trx /tmp/linux.trx
			echo "---- unzip OK ----" >> /tmp/webs_upgrade.log
			sleep 10
		fi	
	fi
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

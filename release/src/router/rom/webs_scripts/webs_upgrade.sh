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
	echo "----------- end of force_free_ram --------------"
}

# get firmware information
echo 3 > /proc/sys/vm/drop_caches
wget $wget_options http://dlcdnet.asus.com/pub/ASUS/wireless/ASUSWRT/$firmware_file -O /tmp/linux.zip
echo 3 > /proc/sys/vm/drop_caches

if [ "$?" != "0" ]; then
	nvram set webs_state_error=1
else
	nvram set webs_state_upgrade=2
	sleep 10
	echo 3 > /proc/sys/vm/drop_caches
	# TODO write to flash
	rc rc_service stop_upgrade
	force_free_ram
	mkdir /tmp/mytmpfs
	mount -t tmpfs -o size=20M,nr_inodes=10k,mode=700 tmpfs /tmp/mytmpfs
	unzip -o /tmp/linux.zip -d /tmp/mytmpfs/
	echo 3 > /proc/sys/vm/drop_caches
	rm /tmp/linux.zip
	mv /tmp/mytmpfs/*.trx /tmp/linux.trx
	sleep 3
	umount /tmp/mytmpfs
	echo 3 > /proc/sys/vm/drop_caches
	rc rc_service start_upgrade
fi

nvram set webs_state_upgrade=1




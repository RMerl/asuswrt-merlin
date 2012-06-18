#!/bin/sh
# $1: filesystem type, $2: device path.

if [ -z "$1" ] || [ -z "$2" ]; then
	echo "Usage: app_fsck.sh [filesystem type] [device's path]"
	exit 0
fi

port_num=`nvram get ehci_ports |awk '{print NF}'`
pool_name=`echo "$2" |awk '{FS="/";print $3}'`
disk_name=`echo "$pool_name" |awk '{FS="[1-9]";print $1}'`
if [ -z "$port_num" ]; then
	port_num=0
fi

usb_path=
got_usb_path=0
p=1
while [ $p -le $port_num ]; do
	usb_path=usb_path$p
	nvram=${usb_path}_act
	nvram_value=`nvram get $nvram`
	if [ "$nvram_value" == "$disk_name" ]; then
		got_usb_path=1
		break
	fi
	p=$(($p+1))
done
if [ $got_usb_path -eq 0 ]; then
	echo "Couldn't get the nvram of disk."
	exit 0
fi

got_fs_path=0
i=0
while [ $i -le 15 ]; do
	nvram=${usb_path}_fs_path$i
	nvram_value=`nvram get $nvram`
	echo "compare: nvram_value=$nvram_value, pool_name=$pool_name."
	if [ "$nvram_value" == "$pool_name" ]; then
		got_fs_path=1
		break
	fi
	i=$(($i+1))
done
if [ $got_fs_path -eq 0 ]; then
	echo "Couldn't get the nvram of pool."
	exit 0
fi

nvram set ${usb_path}_fs_path${i}_error=2
if [ "$1" == "ntfs" ] || [ "$1" == "ufsd" ]; then
	chkntfs -a $2 > /dev/null
else
	fsck.$1 -n $2 > /dev/null
fi

if [ $? -ne 0 ]; then
	nvram set ${usb_path}_fs_path${i}_error=1
else
	nvram set ${usb_path}_fs_path${i}_error=0
fi

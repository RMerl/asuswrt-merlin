#!/bin/sh
# $1: filesystem type, $2: device path.


if [ -z "$1" ] || [ -z "$2" ]; then
	echo "Usage: app_fsck.sh [filesystem type] [device's path]"
	exit 0
fi

port_num=`nvram get ehci_ports |awk '{print NF}'`
pool_name=`echo "$2" |awk '{FS="/"; print $NF}'`
disk_name=`echo "$pool_name" |awk '{FS="[1-9]"; print $1}'`
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
	p=$((p+1))
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
	i=$((i+1))
done
if [ $got_fs_path -eq 0 ]; then
	echo "Couldn't get the nvram of pool."
	exit 0
fi

log_option="> /dev/null"
if [ -e "/usr/bin/logger" ] ; then
	log_option="2>&1 | logger"
fi

set -o pipefail
nvram set ${usb_path}_fs_path${i}_error=2
if [ "$1" == "ntfs" ]; then
	c=0
	RET=1
	while [ ${c} -lt 4 -a ${RET} -ne 0 ] ; do
		c=$((c+1))
		eval chkntfs -a -f --verbose $2 $log_option
		RET=$?
		if [ ${RET} -ge 251 -a ${RET} -le 254 ] ; then
			break
		fi
	done
elif [ "$1" == "hfs" ] || [ "$1" == "hfs+j" ] || [ "$1" == "hfs+jx" ]; then
	c=0
	RET=1
	while [ ${c} -lt 4 -a ${RET} -ne 0 ] ; do
		c=$((c+1))
		eval chkhfs -a -f --verbose $2 $log_option
		RET=$?
		if [ ${RET} -ge 251 -a ${RET} -le 254 ] ; then
			break
		fi
	done
else
	eval fsck.$1 -v $2 $log_option
	RET=$?
fi

if [ ${RET} -ne 0 ]; then
	nvram set ${usb_path}_fs_path${i}_error=1
else
	nvram set ${usb_path}_fs_path${i}_error=0
fi

#!/bin/bash
# This script can be used to quickly switch on usb suspend 
# power saving mode on the Option modem in sysfs 
#
# checking for root
USERID=`id -u`
if [ "$USERID" != "0" ]
then
	echo "Need root permissions to run this script"
	exit
fi
echo $1 >`find /sys -name idVendor | xargs -n 1 grep -H af0 | sed  "s/idVendor:0af0/power\/level/"`
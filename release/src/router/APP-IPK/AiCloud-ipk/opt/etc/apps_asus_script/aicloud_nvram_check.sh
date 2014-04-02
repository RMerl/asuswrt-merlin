#!/bin/sh

touch /tmp/aicloud_check.control
dir_control_file=/tmp/webDAV.conf
GET_VALUE_FROM_NVRAM=`nvram get $1`
if [ -n "$GET_VALUE_FROM_NVRAM" ];then
	echo $1"="$GET_VALUE_FROM_NVRAM>$dir_control_file
else
	echo $1"=">$dir_control_file
fi
rm -rf /tmp/aicloud_check.control

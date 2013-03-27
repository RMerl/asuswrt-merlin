#!/bin/sh
touch /tmp/aicloud_check.control
GET_VALUE_FROM_FILE=`cat /tmp/webDAV.conf | grep $1 | awk '{FS="=";print $2}'`
GET_VALUE_FROM_NVRAM=`nvram get $1`
if [ "$GET_VALUE_FROM_FILE" == "$GET_VALUE_FROM_NVRAM" ];then
	rm -rf /tmp/aicloud_check.control
	exit 0	
else
	if [ "$1" == "webdav_last_login_info" ];then
		GET_VALUE_FROM_NVRAM_TMP=`echo $GET_VALUE_FROM_NVRAM | sed -n 's/\//\\\\\//g'`
		sed -i "s/^$1.*/$1=$GET_VALUE_FROM_NVRAM_TMP/g" /tmp/webDAV.conf
	else
		sed -i "s/^$1.*/$1=$GET_VALUE_FROM_NVRAM/g" /tmp/webDAV.conf
	fi
	rm -rf /tmp/aicloud_check.control
fi
rm -rf /tmp/aicloud_check.control

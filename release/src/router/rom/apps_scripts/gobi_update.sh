#!/bin/sh
PATH=/usr/bin:/bin:/usr/sbin:/sbin


echo "LTE: ### START ###" | logger ;

if [ "`nvram get usb_path1`" != "storage" ] ; then
	echo "LTE: No USB storage is found !" | logger
	exit 1;
fi

path_dev=`nvram get usb_path1_fs_path0`
mounted_path=`mount |grep $path_dev |awk '{print $3}'`

FOLDER=$mounted_path/4G-AC55U_LTE

if [ ! -d ${FOLDER} ] ; then
	echo "LTE: Folder /4G-AC55U_LTE/ not found !" | logger
	exit 1;
fi
cd ${FOLDER}


if [ ! -f version ] ; then 
	echo "LTE: ## STOP ### version file not exist !!" | logger ;
	exit 1 ;
elif [ ! -f update.zip ] ; then
	echo "LTE: ### STOP ###   update.zip file not exist !!" | logger ;
	exit 1 ;
elif [ ! -f update.md5 ] ; then
	echo "LTE: ### STOP ###   update.md5 file not exist !!" | logger ;
	exit 1 ;
elif [ `md5sum -s -c update.md5` ] ; then
	echo "LTE: ### STOP ###   md5sum fail" | logger ;
	exit 1 ;
elif [ `ATE Get_GobiVersion > /tmp/GobiVer` ] ; then 
	echo "LTE: Cannot get Gobi Version" | logger ;
	exit 0 ;
elif [ `cat /tmp/GobiVer` = `cat version` ] ; then 
	echo "LTE: Up to date. Exit" | logger ;
	exit 0 ;
elif [ "`ATE Get_GobiSimCard`" = "PASS" ] ; then
	echo "LTE: ### STOP ###   Remove SIM Card and try again" | logger ;
	exit 1 ;
else
	insmod cdc_ether.ko
	sleep 1 ;

	echo "LTE: switch Gobi mode to internal network" | logger ;
	chat -e '' "at+cfotamode" OK  >> /dev/ttyACM1 < /dev/ttyACM1 ;
	echo $? | logger ;
	sleep 3 ;

	echo "LTE: Gobi internal network: ifup" | logger ;
	ifconfig usb0 up ;
	echo $? | logger ;

	echo "LTE: Gobi internal network: getting IP" | logger ;
	udhcpc -i usb0 -p /var/run/udhcpc1.pid -s /tmp/udhcpc -O33 -O249 -q ;
	echo $? | logger ;
	sleep 3 ;

	chat -e '' "at+cexecfile=rm -rf /cache/" OK  >> /dev/ttyACM1 < /dev/ttyACM1
	sleep 1

	echo "LTE: Gobi internal network: uploading update.zip" | logger ;
#	tftp -p -l update.zip -b 32768 192.168.225.1
	tftp -p -l update.zip 192.168.225.1
	echo $? | logger ;
	sleep 3 ;

	echo "LTE: Gobi internal network: uploading update.md5" | logger ;
#	tftp -p -l update.md5 -b 32768 192.168.225.1
	tftp -p -l update.md5 192.168.225.1
	echo $? | logger ;
	sleep 1 ;

	echo "LTE: updating from `cat /tmp/GobiVer` to `cat version`" | logger ;
	chat -e '' "at+cfotaupdate" OK  >> /dev/ttyACM1 < /dev/ttyACM1
	echo $? | logger ;

	echo "LTE: Waiting for active" | logger ;
	sleep 120 ;

	if [ `grep -q GobiNet /proc/bus/usb/devices` ] ; then
		echo "LTE: ### FAIL ###" | logger ;
		exit 1;
	elif [ `ATE Get_GobiVersion` != `cat version` ] ; then 
		echo "LTE: ### FAIL version incorrect ###" | logger ;
		exit 1;
	else
		echo "LTE: ### SUCCESS ###" | logger ;
	fi
fi

echo "LTE: ### Finish ###" | logger ;

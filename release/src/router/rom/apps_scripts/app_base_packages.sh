#!/bin/sh
# $1: device name.


apps_ipkg_old=`nvram get apps_ipkg_old`

autorun_file=.asusrouter
nonautorun_file=$autorun_file.disabled
APPS_INSTALL_FOLDER=`nvram get apps_install_folder`
APPS_DEV=`nvram get apps_dev`
ASUS_SERVER=`nvram get apps_ipkg_server`
wget_timeout=`nvram get apps_wget_timeout`
#wget_options="-nv -t 2 -T $wget_timeout --dns-timeout=120"
wget_options="-q -t 2 -T $wget_timeout"
apps_from_internet=`nvram get rc_support |grep appnet`
apps_local_space=`nvram get apps_local_space`
apps_local_test=`nvram get apps_local_test`
link_internet=`nvram get link_internet`
f=`nvram get apps_install_folder`

#2016.7.1 sherry new oleg arm{
apps_new_arm=`nvram get apps_new_arm` 
#end sherry add }


case $f in
	"asusware.arm")
		pkg_type=`echo $f|sed -e "s,asusware\.,,"`
		#2016.7.1 sherry new oleg arm{
		#third_lib="mbwe-bluering"  
		if [ $apps_new_arm -eq 1 ]; then 
			third_lib="armeabi-ng"
		else
			third_lib="mbwe-bluering" 
			base_size=503297
		fi #end sherry modify}
		
		;;
	"asusware.big")
		pkg_type="mipsbig"
		third_lib=
		base_size=2004975
		;;
	"asusware.mipsbig")
		pkg_type=`echo $f|sed -e "s,asusware\.,,"`
		third_lib=
		base_size=2004975
		;;
	"asusware")
		pkg_type="mipsel"
		third_lib="oleg"
		if [ -n "$apps_ipkg_old" ] && [ "$apps_ipkg_old" == "1" ]; then
			base_size=1042891
		else
			base_size=936886
		fi
		;;
	*)
		echo "Unknown apps_install_folder: $f"
		exit 1
		;;
esac


if [ -z "$APPS_DEV" ]; then
	echo "Wrong"
	APPS_DEV=$1
fi

if [ -z "$APPS_DEV" ] || [ ! -b "/dev/$APPS_DEV" ];then
	echo "Usage: app_base_packages.sh <device name>"
	nvram set apps_state_error=1
	exit 1
fi

APPS_MOUNTED_PATH=`mount |grep "/dev/$APPS_DEV on " |awk '{print $3}'`
if [ -z "$APPS_MOUNTED_PATH" ]; then
	echo "$1 had not mounted yet!"
	nvram set apps_state_error=2
	exit 1
fi

APPS_INSTALL_PATH=$APPS_MOUNTED_PATH/$APPS_INSTALL_FOLDER

#2016.7.1 sherry new oleg arm{
TEMP_BASELEN_FILE=$APPS_INSTALL_PATH/BasePackages.tgz
BASELEN_TGZ=BasePackages.tgz
BASELEN_FILE=BasePackages
#end sherry add }


if [ -L "$APPS_INSTALL_PATH" ] || [ ! -d "$APPS_INSTALL_PATH" ]; then
	echo "Building the base directory!"
	rm -rf $APPS_INSTALL_PATH
	mkdir -p -m 0777 $APPS_INSTALL_PATH
fi

if [ ! -f "$APPS_INSTALL_PATH/$nonautorun_file" ]; then
	rm -rf $APPS_INSTALL_PATH/$autorun_file
	cp -f $apps_local_space/$autorun_file $APPS_INSTALL_PATH
	if [ "$?" != "0" ]; then
		nvram set apps_state_error=100
		exit 1
	fi
else
	rm -rf $APPS_INSTALL_PATH/$autorun_file
fi

had_uclibc=`ls $APPS_INSTALL_PATH/lib/libuClibc-*.so`
if [ ! -f "$APPS_INSTALL_PATH/bin/ipkg" ] || [ -z "$had_uclibc" ]; then
	echo "Installing the base apps!"
	#2016.7.1 sherry new oleg arm{
	#base_file=asus_base_apps_$pkg_type.tgz
	if [ $apps_new_arm -eq 1 ]; then 
		base_file=asus_base_apps_new_$pkg_type.tgz
	else
		base_file=asus_base_apps_$pkg_type.tgz
	fi #end sherry modify
	
	target=$APPS_INSTALL_PATH/$base_file
	CURRENT_PWD=`pwd`
	rm -rf $target

	# TODO: just for test.
	if [ -n "$apps_local_test" ] && [ "$apps_local_test" -eq "1" ]; then
		local=`nvram get lan_ipaddr`
		dl_path="http://$local"
	elif [ "$pkg_type" != "arm" ] && [ -n "$apps_ipkg_old" ] && [ "$apps_ipkg_old" == "1" ]; then
		IS_SUPPORT_SSL=`nvram get rc_support|grep -i HTTPS`
		if [ -n "$IS_SUPPORT_SSL" ]; then
			dl_path=https://dlcdnets.asus.com/pub/ASUS/LiveUpdate/Release/Wireless
			wget_options="$wget_options --no-check-certificate"
		else
			dl_path=http://dlcdnet.asus.com/pub/ASUS/LiveUpdate/Release/Wireless
		fi
	else
		dl_path=$ASUS_SERVER
	fi

	if [ -z "$apps_from_internet" ]; then
		cp -f $apps_local_space/$base_file $target
		cd $APPS_INSTALL_PATH
		tar xzf $base_file # uclibc-opt & ipkg-opt.
		if [ "$?" != "0" ]; then
			cd $CURRENT_PWD
			nvram set apps_state_error=10
			exit 1
		fi
		cp -f $apps_local_space/optware.asus $APPS_INSTALL_PATH/lib/ipkg/lists/
		if [ -n "$third_lib" ]; then # TODO filename /rom/optware.$third_lib need change
			cp -f $apps_local_space/optware.$third_lib $APPS_INSTALL_PATH/lib/ipkg/lists/
		fi
		cd $CURRENT_PWD
		rm -rf $target
	else
		if [ "$link_internet" != "2" ]; then
			echo "Couldn't connect Internet to install the base apps!"
			nvram set apps_state_error=5
			exit 1
		fi

		#2016.7.1 sherry new oleg arm{
		if [ $apps_new_arm -eq 1 ]; then
			echo "wget --spider $wget_options $dl_path/$BASELEN_TGZ"
			wget --spider $wget_options $dl_path/$BASELEN_TGZ
			if [ "$?" != "0" ]; then
				echo "There is no $BASELEN_TGZ from Internet!"
				nvram set apps_state_error=6
				return 1
			fi

			echo "wget -c $wget_options -O $TEMP_BASELEN_FILE $dl_path/$BASELEN_TGZ"
			wget -c $wget_options -O $TEMP_BASELEN_FILE $dl_path/$BASELEN_TGZ &

			wget_pid=`pidof wget`
			if [ -z "$wget_pid" ] || [ $wget_pid -lt 1 ]; then
				echo "Failed to get $BASELEN_TGZ from Internet!"
				nvram set apps_state_error=6
				return 1
			fi
			i=0
			while [ $i -lt $wget_timeout ] && [ ! -f "$TEMP_BASELEN_FILE" ]; do
				i=$((i+1))
				sleep 1
			done
			
			if [ -f "$TEMP_BASELEN_FILE" ]; then
				i=0
				tar xzf $TEMP_BASELEN_FILE -C $APPS_INSTALL_PATH
				while [ $? != 0 ]; do
					i=$((i+1))
					if [ $i -gt 5 ]; then
						echo "Failed to get $BASELEN_TGZ from Internet!"
						nvram set apps_state_error=6
						return 1
					fi
					sleep 1
					tar xzf $TEMP_BASELEN_FILE -C $APPS_INSTALL_PATH
				done
			else
				echo "Failed to get $BASELEN_TGZ from Internet!"
				nvram set apps_state_error=6
				return 1
			fi		

			base_size_tmp=`cat "$APPS_INSTALL_PATH/$BASELEN_FILE" |grep "base_size:"`
			base_size=${base_size_tmp:11}
		
		fi

		#end sherry add}
		
		nvram set apps_download_file=$target
		echo "wget --spider $wget_options $dl_path/$base_file"
		wget --spider $wget_options $dl_path/$base_file
		if [ "$?" != "0" ]; then
			echo "There is no $base_file from Internet!"
			nvram set apps_state_error=6
			return 1
		fi

		echo "wget -c $wget_options $dl_path/$base_file -O $target"
		wget -c $wget_options $dl_path/$base_file -O $target &
		wget_pid=`pidof wget`
		if [ -z "$wget_pid" ] || [ $wget_pid -lt 1 ]; then
			echo "Failed to get $base_file from Internet!"
			nvram set apps_state_error=6
			return 1
		fi
		i=0
		while [ $i -lt $wget_timeout ] && [ ! -f "$target" ]; do
			i=$((i+1))
			sleep 1
		done

		wget_pid=`pidof wget`
		target_size=`ls -l $target |awk '{printf $5}'`
		percent=$((target_size*100/base_size))
		nvram set apps_download_percent=$percent
		while [ -n "$wget_pid" ] && [ -n "$target_size" ] && [ $target_size -lt $base_size ]; do
			sleep 1

			wget_pid=`pidof wget`
			target_size=`ls -l $target |awk '{printf $5}'`
			percent=$((target_size*100/base_size))
			nvram set apps_download_percent=$percent
		done

		target_size=`ls -l $target |awk '{printf $5}'`
		percent=$((target_size*100/base_size))
		nvram set apps_download_percent=$percent
		if [ -z "$percent" ] || [ $percent -ne 100 ]; then
			echo "Couldn't complete to download $base_file from Internet!"
			nvram set apps_state_error=6
			return 1
		fi

		cd $APPS_INSTALL_PATH
		tar xzf $base_file # uclibc-opt & ipkg-opt.
		if [ "$?" != "0" ]; then
			cd $CURRENT_PWD
			nvram set apps_state_error=10
			exit 1
		fi

		cd $CURRENT_PWD
		rm -rf $target
	fi

	if [ -n "$apps_local_test" ] && [ "$apps_local_test" -eq "1" ]; then
		sed -i '3c src/gz optware.asus '"$dl_path" $APPS_INSTALL_PATH/etc/ipkg.conf
#	elif [ "$pkg_type" != "arm" ] && [ -z "$apps_from_internet" ]; then
	elif [ "$pkg_type" != "arm" ]; then
#		if [ -z "$apps_ipkg_old" ] || [ "$apps_ipkg_old" != "1" ]; then
			sed -i '3c src/gz optware.asus '"$ASUS_SERVER" $APPS_INSTALL_PATH/etc/ipkg.conf
#		fi
	fi
fi

APPS_MOUNTED_TYPE=`mount |grep "/dev/$APPS_DEV on " |awk '{print $5}'`
if [ "$APPS_MOUNTED_TYPE" == "vfat" ] || [ "$APPS_MOUNTED_TYPE" == "tfat" ]; then
	/usr/sbin/app_move_to_pool.sh $APPS_DEV
	if [ "$?" != "0" ]; then
		# apps_state_error was already set by app_move_to_pool.sh.
		exit 1
	fi
fi

#2016.7.6 sherry add for new arm{
if [ $apps_new_arm -eq 1 ]; then
	rm -f $TEMP_BASELEN_FILE
	rm -f $APPS_INSTALL_PATH/$BASELEN_FILE
fi
#end sherry add

/usr/sbin/app_base_link.sh
if [ "$?" != "0" ]; then
	# apps_state_error was already set by app_base_link.sh.
	exit 1
fi

nvram set apps_download_file=
nvram set apps_download_percent=

echo "Success to build the base environment!"

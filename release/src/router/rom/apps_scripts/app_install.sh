#!/bin/sh
# ASUS app install script
# $1: package name, $2: device name.


apps_ipkg_old=`nvram get apps_ipkg_old`
APPS_PATH=/opt
CONF_FILE=$APPS_PATH/etc/ipkg.conf
ASUS_SERVER=`nvram get apps_ipkg_server`
wget_timeout=`nvram get apps_wget_timeout`
#wget_options="-nv -t 2 -T $wget_timeout --dns-timeout=120"
wget_options="-q -t 2 -T $wget_timeout"
download_file=
apps_from_internet=`nvram get rc_support |grep appnet`
apps_local_space=`nvram get apps_local_space`
f=`nvram get apps_install_folder`
apps_new_arm=`nvram get apps_new_arm`  #sherry add 2016.7.5
case $f in
	"asusware.arm")
		pkg_type=`echo $f|sed -e "s,asusware\.,,"`		
		#2016.7.5 sherry new oleg arm{
		#third_lib="mbwe-bluering"  
		if [ $apps_new_arm -eq 1 ]; then 
			third_lib="armeabi-ng"
		else
			third_lib="mbwe-bluering" 
		fi #end sherry modify}
		;;
	"asusware.big")
		pkg_type="mipsbig"
		third_lib=
		;;
	"asusware.mipsbig")
		pkg_type=`echo $f|sed -e "s,asusware\.,,"`
		third_lib=
		;;
	"asusware")
		pkg_type="mipsel"
		third_lib="oleg"
		;;
	*)
		echo "Unknown apps_install_folder: $f"
		exit 1
		;;
esac


# $1: package name.
# return value. 1: have package. 0: no package.
_check_package(){
	package_ready=`ipkg list_installed | grep "$1 "`
	package_ready2=`/usr/sbin/app_get_field.sh $1 Enabled 1`

	if [ -z "$package_ready" ] && [ -z "$package_ready2" ]; then
		return 0
	else
		return 1
	fi
}

# $1: package name, $2: ipkg server name, $3: force(1/0).
_get_pkg_file_name_old(){
	pkg_file_full=`/usr/sbin/app_get_field.sh $1 Filename 2`
	old_pkg_file=`echo "$pkg_file_full" |awk '{FS=".ipk";print $1}'`
	pkg_file=`echo "$old_pkg_file" |sed 's/\./-/g'`

	if [ "$3" == "1" ] || [ "$2" != "$ASUS_SERVER" ]; then
		echo "$pkg_file_full"
	else
		echo "$pkg_file.tgz"
	fi
}

# $1: package name.
_get_pkg_file_name(){
	pkg_file_full=`/usr/sbin/app_get_field.sh $1 Filename 2`

	echo "$pkg_file_full"
}

# $1: ipkg log file, $2: the depends of package.
_check_log_message(){
	got_log=`cat $1 |sed -n '$p'`
	action=`echo $got_log |awk '{print $1}'`

	if [ "$action" == "Installing" ] || [ "$action" == "Configuring" ]; then
		target=`echo $got_log |awk '{print $2}'`
	elif [ "$action" == "Downloading" ]; then
		target=`echo $got_log |awk '{print $2}' |awk '{FS="/"; print $NF}' |awk '{FS="_"; print $1}'`
	elif [ "$action" == "Successfully" ]; then
		target="terminated"
	elif [ "$action" == "update-alternatives:" ]; then
		target=""
	elif [ -z "$action" ]; then
		target="Space"
	else
		target="error"
	fi

	got_target=0
	if [ "$action" == "Installing" ] || [ "$action" == "Configuring" ] || [ "$action" == "Downloading" ]; then
		check_array=`echo $2 |sed 's/,/ /g'`
		for check in $check_array; do
			if [ "$target" == "$check" ]; then
				got_target=1
				break
			fi
		done
	fi

	if [ "$got_target" -eq "1" ]; then
		nvram set apps_depend_action="$action"
		nvram set apps_depend_action_target="$target"
	fi

	echo "$target"

	return 0
}

# $1: delay number.
_loop_delay(){
	i=0
	while [ $i -lt $1 ]; do
		i=$((i+1))
		echo "."
	done
}

# $1: package name, $2: ipkg log file.
_log_ipkg_install(){
	package_deps=`/usr/sbin/app_get_field.sh $1 Depends 2`
	package_deps=`echo $package_deps |sed 's/,/ /g'`
	package_deps_do=

	for dep in $package_deps; do
		_check_package $dep
		if [ "$?" == "1" ]; then
			continue
		fi

		if [ -z "$package_deps_do" ]; then
			package_deps_do=$dep
			nvram set apps_depend_action="$dep"
			nvram set apps_depend_action_target="Installing"
		else
			package_deps_do=$package_deps_do,$dep
		fi
	done
	nvram set apps_depend_do="$package_deps_do"

	ret=`_check_log_message "$2" "$package_deps_do"`
	while [ "$ret" != "terminated" ] && [ "$ret" != "error" ]; do
		_loop_delay 10
		ret=`_check_log_message "$2" "$package_deps_do"`
	done

	echo "$ret"

	return 0
}

# $1: package name, $2: mounted path.
_download_package(){
	if [ "$1" == "uclibc-opt" ] || [ "$1" == "ipkg-opt" ]; then
		return 0
	fi

	pkg_server=
	pkg_file=
	installed_ipk_path=
	need_download=1

	if [ "$1" == "downloadmaster" ] && [ -z "$apps_from_internet" ]; then
		installed_ipk_path=`ls $apps_local_space/downloadmaster*`
	elif [ "$1" == "asuslighttpd" ] && [ -z "$apps_from_internet" ]; then
		installed_ipk_path=`ls $apps_local_space/asuslighttpd*`
	fi

	if [ -n "$installed_ipk_path" ]; then
		list_ver4=`/usr/sbin/app_get_field.sh $1 Version 2 |awk '{FS=".";print $4}'`
		file_name=`echo $installed_ipk_path |awk '{FS="/"; print $NF}'`
		file_ver=`echo $file_name |awk '{FS="_"; print $2}'`
		file_ver4=`echo $file_ver |awk '{FS="."; print $4}'`
echo "file_ver4=$file_ver4, list_ver4=$list_ver4."

		if [ -z "$list_ver4" ] || [ "$list_ver4" -le "$file_ver4" ]; then
			need_download=0
		fi
	fi

	if [ "$need_download" == "1" ]; then
		# Geting the app's file name...
		server_names=`grep -n '^src.*' $CONF_FILE |sort -r |awk '{print $3}'`

		if [ "$pkg_type" != "arm" ] && [ -n "$apps_ipkg_old" ] && [ "$apps_ipkg_old" == "1" ]; then
			IS_SUPPORT_SSL=`nvram get rc_support|grep -i HTTPS`
			if [ -n "$IS_SUPPORT_SSL" ]; then
				wget_options="$wget_options --no-check-certificate"
			fi
		fi

		for s in $server_names; do
			if [ "$pkg_type" != "arm" ] && [ -n "$apps_ipkg_old" ] && [ "$apps_ipkg_old" == "1" ]; then
				pkg_file=`_get_pkg_file_name_old $1 $s 0`
			else
				pkg_file=`_get_pkg_file_name $1`
			fi
			wget --spider $wget_options $s/$pkg_file
			if [ "$?" == "0" ]; then
				pkg_server=$s
				break
			fi
			
		done
		if [ -z "$pkg_server" ]; then
			nvram set apps_state_error=6
			return 1
		fi

		# Downloading the app's file name...
		if [ "$pkg_type" != "arm" ] && [ -n "$apps_ipkg_old" ] && [ "$apps_ipkg_old" == "1" ] && [ "$pkg_server" == "$ASUS_SERVER" ]; then
			ipk_file_name=`_get_pkg_file_name_old $1 $pkg_server 1`
		else
			ipk_file_name=$pkg_file
		fi

		target=$2/$ipk_file_name
		nvram set apps_download_file=$ipk_file_name
		nvram set apps_download_percent=0
		echo "wget -c $wget_options $pkg_server/$pkg_file -O $target"
		wget -c $wget_options $pkg_server/$pkg_file -O $target &
		wget_pid=`pidof wget`
		if [ -z "$wget_pid" ] || [ $wget_pid -lt 1 ]; then
			rm -rf $target
			sync

			nvram set apps_state_error=6
			return 1
		fi
		i=0
		while [ $i -lt $wget_timeout ] && [ ! -f "$target" ]; do
			i=$((i+1))
			sleep 1
		done

		wget_pid=`pidof wget`
		size=`/usr/sbin/app_get_field.sh $1 Size 2`
		target_size=`ls -l $target |awk '{printf $5}'`
		percent=$((target_size*100/size))
		nvram set apps_download_percent=$percent
		while [ -n "$wget_pid" ] && [ -n "$target_size" ] && [ $target_size -lt $size ]; do
			sleep 1

			wget_pid=`pidof wget`
			target_size=`ls -l $target |awk '{printf $5}'`
			percent=$((target_size*100/size))
			nvram set apps_download_percent=$percent
		done

		target_size=`ls -l $target |awk '{printf $5}'`
		percent=$((target_size*100/size))
		nvram set apps_download_percent=$percent
		if [ -z "$percent" ] || [ $percent -ne 100 ]; then
			rm -rf $target
			sync

			nvram set apps_state_error=6
			return 1
		fi

		installed_ipk_path=$2"/"$ipk_file_name
	fi

	download_file=$installed_ipk_path

	return 0
}

#sherry add for UI app_state{ 
# $1: ipkg log file
_check__lib_log_message(){

	got_log=`cat $1 |sed -n '$p'`
	action=`echo $got_log |awk '{print $1}'`

	if [ "$action" == "Installing" ] || [ "$action" == "Configuring" ]; then
		target=`echo $got_log |awk '{print $2}'`
	elif [ "$action" == "Downloading" ]; then
		target=`echo $got_log |awk '{print $2}' |awk '{FS="/"; print $NF}' |awk '{FS="_"; print $1}'`
	elif [ "$action" == "Successfully" ]; then
		target="terminated"
	elif [ "$action" == "update-alternatives:" ]; then
		target=""
	elif [ -z "$action" ]; then
		target="Space"
	else
		target="error"
	fi

	if [ "$action" == "Installing" ] || [ "$action" == "Configuring" ] || [ "$action" == "Downloading" ]; then
		nvram set apps_depend_action="$action"
		nvram set apps_depend_action_target="$target"
	fi

	echo "$target"

	return 0

}

#instead "_log_ipkg_install()" when install dependent lib 
# $1: package name (all path), $2: ipkg log file.
_log_lib_ipkg_install(){

	package_deps=`echo "$1"|awk '{FS="/"; print $NF}'|awk '{FS="_";printf $1}'`
	nvram set apps_depend_action="$package_deps"
	nvram set apps_depend_action_target="Installing"
	
	ret=`_check__lib_log_message "$2"`
	while [ "$ret" != "terminated" ] && [ "$ret" != "error" ]; do
		_loop_delay 10
		ret=`_check__lib_log_message "$2"`
	done

	echo "$ret"

	return 0
}

#instead "_download_package()" when download dependent lib 
# $1: package name, $2: mounted path.
_download_lib_package(){
	if [ "$1" == "uclibc-opt" ] || [ "$1" == "ipkg-opt" ]; then
		return 0
	fi

	pkg_server=
	pkg_file=
	installed_ipk_path=
	need_download=1

	if [ "$need_download" == "1" ]; then
		# Geting the app's file name...
		server_names=`grep -n '^src.*' $CONF_FILE |awk '{print $3}'`

		if [ "$pkg_type" != "arm" ] && [ -n "$apps_ipkg_old" ] && [ "$apps_ipkg_old" == "1" ]; then
			IS_SUPPORT_SSL=`nvram get rc_support|grep -i HTTPS`
			if [ -n "$IS_SUPPORT_SSL" ]; then
				wget_options="$wget_options --no-check-certificate"
			fi
		fi

		for s in $server_names; do
			if [ "$pkg_type" != "arm" ] && [ -n "$apps_ipkg_old" ] && [ "$apps_ipkg_old" == "1" ]; then
				pkg_file=`_get_pkg_file_name_old $1 $s 0`
			else
				pkg_file=`_get_pkg_file_name $1`
			fi
			
			wget --spider $wget_options $s/$pkg_file
			if [ "$?" == "0" ]; then
				pkg_server=$s
				break
			fi
		done
		if [ -z "$pkg_server" ]; then
			nvram set apps_state_error=6
			return 1
		fi

		# Downloading the app's file name...
		if [ "$pkg_type" != "arm" ] && [ -n "$apps_ipkg_old" ] && [ "$apps_ipkg_old" == "1" ] && [ "$pkg_server" == "$ASUS_SERVER" ]; then
			ipk_file_name=`_get_pkg_file_name_old $1 $pkg_server 1`
		else
			ipk_file_name=$pkg_file
		fi

		target=$2/$ipk_file_name
		#nvram set apps_download_file=$ipk_file_name
		#nvram set apps_download_percent=0
		echo "wget -c $wget_options $pkg_server/$pkg_file -O $target"
		wget -c $wget_options $pkg_server/$pkg_file -O $target &
		wget_pid=`pidof wget`
		if [ -z "$wget_pid" ] || [ $wget_pid -lt 1 ]; then
			rm -rf $target
			sync

			nvram set apps_state_error=6
			return 1
		fi
		i=0
		while [ $i -lt $wget_timeout ] && [ ! -f "$target" ]; do
			i=$((i+1))
			sleep 1
		done

		wget_pid=`pidof wget`
		size=`/usr/sbin/app_get_field.sh $1 Size 2`
		target_size=`ls -l $target |awk '{printf $5}'`
		percent=$((target_size*100/size))
		#nvram set apps_download_percent=$percent
		while [ -n "$wget_pid" ] && [ -n "$target_size" ] && [ $target_size -lt $size ]; do
			sleep 1

			wget_pid=`pidof wget`
			target_size=`ls -l $target |awk '{printf $5}'`
			percent=$((target_size*100/size))
			#nvram set apps_download_percent=$percent
		done

		target_size=`ls -l $target |awk '{printf $5}'`
		percent=$((target_size*100/size))
		#nvram set apps_download_percent=$percent
		if [ -z "$percent" ] || [ $percent -ne 100 ]; then
			rm -rf $target
			sync

			nvram set apps_state_error=6
			return 1
		fi

		installed_ipk_path=$2"/"$ipk_file_name
	fi

	download_file=$installed_ipk_path

	return 0
}

#end sherry add }

nvram set apps_state_install=0 # INITIALIZING
nvram set apps_state_error=0

autorun_file=.asusrouter
nonautorun_file=$autorun_file.disabled
APPS_INSTALL_FOLDER=`nvram get apps_install_folder`
SWAP_ENABLE=`nvram get apps_swap_enable`
SWAP_THRESHOLD=`nvram get apps_swap_threshold`
SWAP_FILE=`nvram get apps_swap_file`
SWAP_SIZE=`nvram get apps_swap_size`
PATH=$APPS_PATH/usr/bin:$APPS_PATH/bin:$APPS_PATH/usr/sbin:$APPS_PATH/sbin:/usr/bin:/bin:/usr/sbin:/sbin
unset LD_LIBRARY_PATH
unset LD_PRELOAD

if [ -z "$1" ]; then
	echo "Usage: app_install.sh <Package name> [device name]"
	nvram set apps_state_error=1
	exit 1
fi

APPS_DEV=`nvram get apps_dev`
APPS_MOUNTED_PATH=`nvram get apps_mounted_path`
if [ -z "$APPS_DEV" ]; then
	APPS_DEV=$2

	if [ -z "$APPS_DEV" ] || [ ! -b "/dev/$APPS_DEV" ];then
		echo "Usage: app_install.sh <Package name> [device name]"
		nvram set apps_state_error=1
		exit 1
	fi

	echo "Initializing the APP environment..."

	APPS_MOUNTED_PATH=`mount |grep "/dev/$2 on " |awk '{print $3}'`
	if [ -z "$APPS_MOUNTED_PATH" ]; then
		echo "$2 had not mounted yet!"
		nvram set apps_state_error=1
		exit 1
	fi

	nvram set apps_dev=$APPS_DEV
	nvram set apps_mounted_path=$APPS_MOUNTED_PATH
fi
APPS_INSTALL_PATH=$APPS_MOUNTED_PATH/$APPS_INSTALL_FOLDER


nvram set apps_state_install=1 # CHECKING_PARTITION
#sherry add for 2016.7.18
APPS_INSTALL_FOLDER_BAK=$APPS_INSTALL_FOLDER".bak"
APPS_INSTALL_PATH_BAK=$APPS_MOUNTED_PATH/$APPS_INSTALL_FOLDER_BAK
ASUS_UCLIBC_VER=1.0.12-1
dm_exist=
aicloud_exist=
ms_exist=
if [ $apps_new_arm -eq 1 ]; then 
	uclibc_control_file=$APPS_INSTALL_PATH/lib/ipkg/info/uclibc-opt.control

	if [ -f "$uclibc_control_file" ]; then
		uclibc_version=`cat "$uclibc_control_file" |grep "Version:"`
		uclibc_version=${uclibc_version:9}
		ASUS_UCLIBC_VER_NUM=`echo $ASUS_UCLIBC_VER |sed 's/\.//g'|sed 's/\-//g'`
		uclibc_version_num=`echo $uclibc_version |sed 's/\.//g'|sed 's/\-//g'`
		if [ $ASUS_UCLIBC_VER_NUM -gt $uclibc_version_num ]; then
			app_init_run.sh allpkg stop
			if [ -f $APPS_INSTALL_PATH/lib/ipkg/info/downloadmaster.control ]; then
				dm_exist=1
			fi

			if [ -f $APPS_INSTALL_PATH/lib/ipkg/info/aicloud.control ]; then
				aicloud_exist=1
			fi

			if [ -f $APPS_INSTALL_PATH/lib/ipkg/info/mediaserver.control ]; then
				ms_exist=1
			fi

			cp -rf $APPS_INSTALL_PATH $APPS_INSTALL_PATH_BAK
			rm -rf $APPS_INSTALL_PATH

			app_base_packages.sh $APPS_DEV
			if [ "$?" != "0" ]; then
				exit 1
			fi
			app_update.sh

			app_inst_error=0
			dm_inst_error=1
			ms_inst_error=1
			aicloud_inst_error=1
			i=0

			while [ "$i" == "0" -o "$app_inst_error" == "1" ]; do
	
				i=$(($i+1))
				if [ "$i" -gt "2" ]; then
					break
				fi
				
				if [ "$1" != "downloadmaster" ] && [ "$dm_exist" == "1" ] && [ "$dm_inst_error" == "1" ]; then
					app_install.sh downloadmaster $APPS_DEV
					echo $?
					if [ "$?" == "0" ]; then
						dm_inst_error=0
					else
						app_inst_error=1
					fi
				fi

				
				if [ "$1" != "aicloud" ] && [ "$aicloud_exist" == "1" ] && [ "$aicloud_inst_error" == "1" ]; then
					app_install.sh aicloud $APPS_DEV
					if [ "$?" == "0" ]; then
						aicloud_inst_error=0
					else
						app_inst_error=1
					fi
				fi

				
				if [ "$1" != "mediaserver" ] && [ "$ms_exist" == "1" ] && [ "$ms_inst_error" == "1" ]; then
					app_install.sh mediaserver $APPS_DEV
					if [ "$?" == "0" ]; then
						ms_inst_error=0
					else
						app_inst_error=1
					fi
				fi

			done

			if [ "$app_inst_error" == "1" ]; then #install error
				rm -rf $APPS_INSTALL_PATH
				cp -rf $APPS_INSTALL_PATH_BAK $APPS_INSTALL_PATH
				rm -rf $APPS_INSTALL_PATH_BAK
				app_init_run.sh allpkg start
				exit 1
			else
				rm -rf $APPS_INSTALL_PATH_BAK
			fi


		fi
	fi

fi

#end sherry 2016.7.18

/usr/sbin/app_base_packages.sh $APPS_DEV
if [ "$?" != "0" ]; then
	# apps_state_error was already set by app_base_packages.sh.
	exit 1
fi

_check_package $1
if [ "$?" != "0" ]; then
	echo "The \"$1\" is installed already!"
	/usr/sbin/app_init_run.sh allpkg restart
	nvram set apps_state_install=5 # FINISHED
	exit 0
fi


nvram set apps_state_install=2 # CHECKING_SWAP
if [ "$SWAP_ENABLE" != "1" ]; then
	echo "Disable to swap!"
else
	mem_size=`free |sed '1,3d' |awk '{print $4}'`
	if [ "$SWAP_THRESHOLD" == "" ] || [ $mem_size -lt $SWAP_THRESHOLD ]; then
		pool_size=`df /dev/$APPS_DEV |sed '1d' |awk '{print $4}'`
		if [ $pool_size -gt $SWAP_SIZE ]; then
			if [ -e "$APPS_INSTALL_PATH/$SWAP_FILE" ]; then
				swapoff $APPS_INSTALL_PATH/$SWAP_FILE
				rm -rf $APPS_INSTALL_PATH/$SWAP_FILE
			fi

			swap_count=`expr $SWAP_SIZE / 1000 - 1`
			echo "dd if=/dev/zero of=$APPS_INSTALL_PATH/$SWAP_FILE bs=1M count=$swap_count"
			dd if=/dev/zero of=$APPS_INSTALL_PATH/$SWAP_FILE bs=1M count=$swap_count
			echo "mkswap $APPS_INSTALL_PATH/$SWAP_FILE"
			mkswap $APPS_INSTALL_PATH/$SWAP_FILE
			echo "swapon $APPS_INSTALL_PATH/$SWAP_FILE"
			swapon $APPS_INSTALL_PATH/$SWAP_FILE
		else
			echo "No enough partition size!"
			nvram set apps_state_error=3
			exit 1
		fi
	fi
fi


nvram set apps_state_install=3 # DOWNLOADING
link_internet=`nvram get link_internet`
if [ "$link_internet" != "2" ]; then
	cp -f $apps_local_space/optware.asus $APPS_INSTALL_PATH/lib/ipkg/lists/
	if [ -n "$third_lib" ]; then
		cp -f $apps_local_space/optware.$third_lib $APPS_INSTALL_PATH/lib/ipkg/lists/
	fi
elif [ "$1" == "downloadmaster" ] && [ -z "$apps_from_internet" ]; then
	/usr/sbin/app_update.sh optware.asus
	if [ -n "$third_lib" ]; then
		cp -f $apps_local_space/optware.$third_lib $APPS_INSTALL_PATH/lib/ipkg/lists/
	fi
else
	/usr/sbin/app_update.sh
fi

need_asuslighttpd=0
need_asusffmpeg=0
need_smartsync=0
if [ "$1" == "downloadmaster" ]; then
	DM_version1=`/usr/sbin/app_get_field.sh downloadmaster Version 2 |awk '{FS=".";print $1}'`
	DM_version4=`/usr/sbin/app_get_field.sh downloadmaster Version 2 |awk '{FS=".";print $4}'`

	if [ "$DM_version1" -gt "3" ]; then
		need_asuslighttpd=1
	elif [ "$DM_version1" -eq "3" ] && [ "$DM_version4" -gt "59" ]; then
		need_asuslighttpd=1
	fi
elif [ "$1" == "mediaserver" ]; then
	MS_version1=`/usr/sbin/app_get_field.sh mediaserver Version 2 |awk '{FS=".";print $1}'`
	MS_version4=`/usr/sbin/app_get_field.sh mediaserver Version 2 |awk '{FS=".";print $4}'`

	if [ "$MS_version1" -gt "1" ]; then
		need_asuslighttpd=1
	elif [ "$MS_version1" -eq "1" ] && [ "$MS_version4" -gt "15" ]; then
		need_asuslighttpd=1
	fi

	if [ "$MS_version1" -gt "1" ]; then
		need_asusffmpeg=1
	elif [ "$MS_version1" -eq "1" ] && [ "$MS_version4" -ge "30" ]; then
		need_asusffmpeg=1
	fi
elif [ "$1" == "aicloud" ]; then
	AC_version1=`/usr/sbin/app_get_field.sh aicloud Version 2 |awk '{FS=".";print $1}'`
	AC_version4=`/usr/sbin/app_get_field.sh aicloud Version 2 |awk '{FS=".";print $4}'`

	if [ "$AC_version1" -gt "1" ]; then
		need_smartsync=1
	elif [ "$AC_version1" -eq "1" ] && [ "$AC_version4" -gt "4" ]; then
		need_smartsync=1
	fi
fi

target_file=
if [ "$need_asuslighttpd" == "1" ]; then
	echo "Downloading the dependent package: asuslighttpd..."
	_download_package asuslighttpd $APPS_INSTALL_PATH/tmp
	if [ "$?" != "0" ]; then
		echo "Fail to download the package: asuslighttpd!"
		# apps_state_error was already set by _download_package().
		exit 1
	fi
	if [ -z "$target_file" ]; then
		target_file=$download_file
	else
		target_file=$target_file" $download_file"
	fi
fi
if [ "$need_asusffmpeg" == "1" ]; then
	echo "Downloading the dependent package: asusffmpeg..."
	_download_package asusffmpeg $APPS_INSTALL_PATH/tmp
	if [ "$?" != "0" ]; then
		echo "Fail to download the package: asusffmpeg!"
		# apps_state_error was already set by _download_package().
		exit 1
	fi
	if [ -z "$target_file" ]; then
		target_file=$download_file
	else
		target_file=$target_file" $download_file"
	fi
fi
if [ "$need_smartsync" == "1" ]; then
	if [ -n "$apps_ipkg_old" ] && [ "$apps_ipkg_old" == "1" ]; then
		deps=`/usr/sbin/app_get_field.sh smartsync Depends 2 |sed 's/,/ /g'`

		for dep in $deps; do
			echo "Downloading the dependent package of smartsync: $dep..."
			_download_package $dep $APPS_INSTALL_PATH/tmp
			if [ "$?" != "0" ]; then
				echo "Fail to download the package: $dep!"
				# apps_state_error was already set by _download_package().
				exit 1
			fi
			if [ -z "$target_file" ]; then
				target_file=$download_file
			else
				target_file=$target_file" $download_file"
			fi
		done
	fi

	echo "Downloading the dependent package: smartsync..."
	_download_package smartsync $APPS_INSTALL_PATH/tmp
	if [ "$?" != "0" ]; then
		echo "Fail to download the package: smartsync!"
		# apps_state_error was already set by _download_package().
		exit 1
	fi
	if [ -z "$target_file" ]; then
		target_file=$download_file
	else
		target_file=$target_file" $download_file"
	fi
fi

echo "Downloading the package: $1..."
_download_package $1 $APPS_INSTALL_PATH/tmp
if [ "$?" != "0" ]; then
	echo "Fail to download the package: $1!"
	# apps_state_error was already set by _download_package().
	exit 1
fi
if [ -z "$target_file" ]; then
	target_file=$download_file
else
	target_file=$target_file" $download_file"
fi
echo "target_file=$target_file..."


nvram set apps_state_install=4 # INSTALLING
if [ "$1" == "downloadmaster" ] && [ -z "$apps_from_internet" ]; then
	echo "downloadmaster_file=$download_file..."
	/usr/sbin/app_base_library.sh $APPS_DEV $download_file
	if [ "$?" != "0" ]; then
		# apps_state_error was already set by app_base_library.sh.
		return 1
	fi
elif [ "$1" == "asuslighttpd" ] && [ -z "$apps_from_internet" ]; then
	/usr/sbin/app_base_library.sh $APPS_DEV
	if [ "$?" != "0" ]; then
		# apps_state_error was already set by app_base_library.sh.
		return 1
	fi
fi

for file in $target_file; do
	echo "Installing the package: $file..."
	install_log=$APPS_INSTALL_PATH/ipkg_log.txt
	#2016.7.5 sherry add install dependent lib{
	if [ $apps_new_arm -eq 1 ]; then 
		file_tmp=`echo "$file"|awk '{FS="/"; print $NF}'|awk '{FS="_";printf $1}'`
		base_library=`/usr/sbin/app_get_field.sh $file_tmp Depends 2 |sed 's/,/ /g'`
		nvram set apps_depend_do="$base_library"
		lib_install_log=$APPS_INSTALL_PATH/lib_ipkg_log.txt
		for lib in $base_library; do
			if [ -n "`echo "$lib"|grep "asus"`" ]; then
				continue
			fi
			_check_package $lib
			if [ "$?" == "1" ]; then
				continue
			fi			
			_download_lib_package $lib $APPS_INSTALL_PATH/tmp

			if [ "$?" != "0" ]; then
				echo "Fail to download the package: $lib!"
				# apps_state_error was already set by _download_package().
				exit 1
			fi
			
			ipkg install $download_file 1>$lib_install_log &

			result=`_log_lib_ipkg_install $download_file $lib_install_log`
			if [ "$result" == "error" ]; then
				echo "Fail to install the package: $file!"
				nvram set apps_state_error=7
				exit 1
			else
				rm -rf $download_file
				rm -f $install_log
			fi
		done
	fi
	#fi #end sherry modify}
	ipkg install $file 1>$install_log &
	#2016.7.5 sherry modify{
	#if [ $apps_new_arm -eq 1 ];then 
		#sed -i 's|^src/gz optware.asus|#src/gz optware.asus|g' $CONF_FILE
		#ipkg install $file 1>$install_log &
	#else
	#	ipkg install $file 1>$install_log & 
	#fi #end sherry modify}
	result=`_log_ipkg_install $1 $install_log`
	if [ "$result" == "error" ]; then
		echo "Fail to install the package: $file!"
		nvram set apps_state_error=7
		exit 1
	else
		rm -rf $file
		rm -f $install_log
	fi
done
#2016.7.5 sherry add{
#if [ $apps_new_arm -eq 1 ];then 
#	sed -i 's|^#src/gz optware.asus|src/gz optware.asus|g' $CONF_FILE
#fi
#end sherry add}

APPS_MOUNTED_TYPE=`mount |grep "/dev/$APPS_DEV on " |awk '{print $5}'`
if [ "$APPS_MOUNTED_TYPE" == "vfat" ] || [ "$APPS_MOUNTED_TYPE" == "tfat" ]; then
	/usr/sbin/app_move_to_pool.sh $APPS_DEV
	if [ "$?" != "0" ]; then
		# apps_state_error was already set by app_move_to_pool.sh.
		exit 1
	fi
fi

/usr/sbin/app_base_link.sh
if [ "$?" != "0" ]; then
	# apps_state_error was already set by app_base_link.sh.
	exit 1
fi

if [ "$need_asuslighttpd" == "1" ]; then
	echo "Enabling the dependent package: asuslighttpd..."
	/usr/sbin/app_set_enabled.sh asuslighttpd "yes"
fi
if [ "$need_asusffmpeg" == "1" ]; then
	echo "Enabling the dependent package: asusffmpeg..."
	/usr/sbin/app_set_enabled.sh asusffmpeg "yes"
fi
if [ "$need_smartsync" == "1" ]; then
	if [ -n "$apps_ipkg_old" ] && [ "$apps_ipkg_old" == "1" ]; then
		deps=`/usr/sbin/app_get_field.sh smartsync Depends 2 |sed 's/,/ /g'`

		for dep in $deps; do
			echo "Enabling the dependent package of smartsync: $dep..."
			/usr/sbin/app_set_enabled.sh $dep "yes"
		done
	fi

	echo "Enabling the dependent package: smartsync..."
	/usr/sbin/app_set_enabled.sh smartsync "yes"
fi

echo "Enabling the package: $1..."
/usr/sbin/app_set_enabled.sh $1 "yes"

if [ "$link_internet" == "2" ]; then
	/usr/sbin/app_update.sh&
fi

test_of_var_files "$APPS_MOUNTED_PATH"
rc rc_service restart_nasapps

nvram set apps_download_file=
nvram set apps_download_percent=

nvram set apps_depend_do=
nvram set apps_depend_action=
nvram set apps_depend_action_target=


nvram set apps_state_install=5 # FINISHED

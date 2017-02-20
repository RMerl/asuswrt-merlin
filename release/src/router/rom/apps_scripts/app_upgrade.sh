#!/bin/sh
# $1: package name.


apps_ipkg_old=`nvram get apps_ipkg_old`
f=`nvram get apps_install_folder`
case $f in
	"asusware.arm")
		pkg_type=`echo $f|sed -e "s,asusware\.,,"`
		;;
	"asusware.big")
		pkg_type="mipsbig"
		;;
	"asusware.mipsbig")
		pkg_type=`echo $f|sed -e "s,asusware\.,,"`
		;;
	"asusware")
		pkg_type="mipsel"
		;;
	*)
		echo "Unknown apps_install_folder: $f"
		exit 1
		;;
esac
APPS_PATH=/opt
CONF_FILE=$APPS_PATH/etc/ipkg.conf
ASUS_SERVER=`nvram get apps_ipkg_server`
wget_timeout=`nvram get apps_wget_timeout`
#wget_options="-nv -t 2 -T $wget_timeout --dns-timeout=120"
wget_options="-q -t 2 -T $wget_timeout"
download_file=
apps_new_arm=`nvram get apps_new_arm`  #sherry add 

# $1: package name.
# return value. 1: have package. 0: no package.
_check_package(){
	package_ready=`ipkg list_installed | grep "$1 "`

	if [ -z "$package_ready" ]; then
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

# $1: package name, $2: installed path.
_download_package(){
	pkg_server=
	pkg_file=

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
		download_file=`_get_pkg_file_name_old $1 $pkg_server 1`
	else
		download_file=$pkg_file
	fi

	target=$2/$download_file
	nvram set apps_download_file=$download_file
	nvram set apps_download_percent=0
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

	return 0
}


nvram set apps_state_upgrade=0 # INITIALIZING
nvram set apps_state_error=0

if [ -z "$1" ]; then
	echo "Usage: app_upgrade.sh <Package name>"
	nvram set apps_state_error=1
	exit 1
fi

version=`/usr/sbin/app_get_field.sh $1 Version 1`
new_version=`/usr/sbin/app_get_field.sh $1 Version 2`

if [ "$version" == "$new_version" ]; then
	echo "The package: $1 is the newest one."
#	nvram set apps_state_upgrade=3 # FINISHED
#	exit 0
fi

APPS_DEV=`nvram get apps_dev`
APPS_MOUNTED_PATH=`nvram get apps_mounted_path`
APPS_INSTALL_FOLDER=`nvram get apps_install_folder`
if [ -z "$APPS_DEV" ] || [ -z "$APPS_MOUNTED_PATH" ]; then
	nvram set apps_state_error=2
	exit 1
fi
APPS_INSTALL_PATH=$APPS_MOUNTED_PATH/$APPS_INSTALL_FOLDER

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

nvram set apps_state_upgrade=1 # DOWNLOADING
target_file=
if [ "$need_asuslighttpd" == "1" ]; then
	echo "Downloading the dependent package: asuslighttpd..."
	_download_package asuslighttpd $APPS_INSTALL_PATH/tmp
	if [ "$?" != "0" ]; then
		# apps_state_error was already set by _download_package().
		exit 1
	fi
	if [ -z "$target_file" ]; then
		target_file=$APPS_INSTALL_PATH/tmp/$download_file
	else
		target_file=$target_file" $APPS_INSTALL_PATH/tmp/$download_file"
	fi
fi
if [ "$need_asusffmpeg" == "1" ]; then
	echo "Downloading the dependent package: asusffmpeg..."
	_download_package asusffmpeg $APPS_INSTALL_PATH/tmp
	if [ "$?" != "0" ]; then
		# apps_state_error was already set by _download_package().
		exit 1
	fi
	if [ -z "$target_file" ]; then
		target_file=$APPS_INSTALL_PATH/tmp/$download_file
	else
		target_file=$target_file" $APPS_INSTALL_PATH/tmp/$download_file"
	fi
fi
if [ "$need_smartsync" == "1" ]; then
	if [ -n "$apps_ipkg_old" ] && [ "$apps_ipkg_old" == "1" ]; then
		deps=`/usr/sbin/app_get_field.sh smartsync Depends 2 |sed 's/,/ /g'`

		for dep in $deps; do
			echo "Downloading the dependent package of smartsync: $dep..."
			_download_package $dep $APPS_INSTALL_PATH/tmp
			if [ "$?" != "0" ]; then
				# apps_state_error was already set by _download_package().
				exit 1
			fi
			if [ -z "$target_file" ]; then
				target_file=$APPS_INSTALL_PATH/tmp/$download_file
			else
				target_file=$target_file" $APPS_INSTALL_PATH/tmp/$download_file"
			fi
		done
	fi

	echo "Downloading the dependent package: smartsync..."
	_download_package smartsync $APPS_INSTALL_PATH/tmp
	if [ "$?" != "0" ]; then
		# apps_state_error was already set by _download_package().
		exit 1
	fi
	if [ -z "$target_file" ]; then
		target_file=$APPS_INSTALL_PATH/tmp/$download_file
	else
		target_file=$target_file" $APPS_INSTALL_PATH/tmp/$download_file"
	fi
fi

_download_package $1 $APPS_INSTALL_PATH/tmp
if [ "$?" != "0" ]; then
	# apps_state_error was already set by _download_package().
	exit 1
fi
if [ -z "$target_file" ]; then
	target_file=$APPS_INSTALL_PATH/tmp/$download_file
else
	target_file=$target_file" $APPS_INSTALL_PATH/tmp/$download_file"
fi


nvram set apps_state_upgrade=2 # REMOVING
_check_package $1
if [ "$?" != "0" ]; then
	/usr/sbin/app_remove.sh $1
	if [ "$?" != "0" ]; then
		# apps_state_error was already set by app_remove.sh.
		exit 1
	fi
fi


nvram set apps_state_upgrade=3 # INSTALLING
for file in $target_file; do
	echo "Installing the package: $file..."
	install_log=$APPS_INSTALL_PATH/ipkg_log.txt
	ipkg install $file 1>$install_log &
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

nvram set apps_download_file=
nvram set apps_download_percent=

nvram set apps_depend_do=
nvram set apps_depend_action=
nvram set apps_depend_action_target=


nvram set apps_state_upgrade=4 # FINISHED

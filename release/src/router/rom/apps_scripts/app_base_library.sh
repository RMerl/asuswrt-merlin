#!/bin/sh
# $1: device name, $2: DM_file.


is_arm_machine=`uname -m |grep arm`
productid=`nvram get productid`

autorun_file=.asusrouter
nonautorun_file=$autorun_file.disabled
APPS_INSTALL_FOLDER=`nvram get apps_install_folder`
APPS_DEV=`nvram get apps_dev`
apps_from_internet=`nvram get rc_support |grep appnet`
apps_local_space=`nvram get apps_local_space`

# $1: package name.
# return value. 1: have package. 0: no package.
_check_package(){
	package_ready=`ipkg list_installed | grep "$1 "`
	package_ready2=`app_get_field.sh $1 Enabled 1`

	if [ -z "$package_ready" ] && [ -z "$package_ready2" ]; then
		return 0
	else
		return 1
	fi
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
		i=$(($i+1))
		echo "."
	done
}

# $1: package name, $2: ipkg log file.
_log_ipkg_install(){
	package_deps=`app_get_field.sh $1 Depends 2`
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

if [ -n "$is_arm_machine" ]; then
	pkg_type="arm"
elif [ -n "$productid" ] && [ "$productid" == "DSL-N66U" ]; then
	pkg_type="mipsbig"
else
	pkg_type="mipsel"
fi

if [ -n "$apps_from_internet" ]; then
	exit 0
fi

if [ -z "$APPS_DEV" ]; then
	echo "Wrong"
	APPS_DEV=$1
fi

if [ -z "$APPS_DEV" ] || [ ! -b "/dev/$APPS_DEV" ];then
	echo "Usage: app_base_library.sh <device name>"
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
if [ -L "$APPS_INSTALL_PATH" ] || [ ! -d "$APPS_INSTALL_PATH" ]; then
	echo "Building the base directory!"
	rm -rf $APPS_INSTALL_PATH
	mkdir -p -m 0777 $APPS_INSTALL_PATH
fi

if [ ! -f "$APPS_INSTALL_PATH/$nonautorun_file" ]; then
	rm -rf $APPS_INSTALL_PATH/$autorun_file
	cp -f $apps_local_space/$autorun_file $APPS_INSTALL_PATH
	if [ "$?" != "0" ]; then
		nvram set apps_state_error=10
		exit 1
	fi
else
	rm -rf $APPS_INSTALL_PATH/$autorun_file
fi

install_log=$APPS_INSTALL_PATH/ipkg_log.txt
list_installed=`ipkg list_installed`

if [ -z "`echo "$list_installed" |grep "openssl - "`" ]; then
	echo "Installing the package: openssl..."
	ipkg install $apps_local_space/openssl_*_$pkg_type.ipk 1>$install_log &
	result=`_log_ipkg_install downloadmaster $install_log`
	if [ "$result" == "error" ]; then
		echo "Failed to install openssl!"
		nvram set apps_state_error=4
		exit 1
	else
		rm -f $install_log
	fi
fi

if [ -z "`echo "$list_installed" |grep "zlib - "`" ]; then
	echo "Installing the package: zlib..."
	ipkg install $apps_local_space/zlib_*_$pkg_type.ipk 1>$install_log &
	result=`_log_ipkg_install downloadmaster $install_log`
	if [ "$result" == "error" ]; then
		echo "Failed to install zlib!"
		nvram set apps_state_error=4
		exit 1
	else
		rm -f $install_log
	fi
fi

if [ -z "`echo "$list_installed" |grep "libcurl - "`" ]; then
	echo "Installing the package: libcurl..."
	ipkg install $apps_local_space/libcurl_*_$pkg_type.ipk 1>$install_log &
	result=`_log_ipkg_install downloadmaster $install_log`
	if [ "$result" == "error" ]; then
		echo "Failed to install libcurl!"
		nvram set apps_state_error=4
		exit 1
	else
		rm -f $install_log
	fi
fi

if [ -z "`echo "$list_installed" |grep "libevent - "`" ]; then
	echo "Installing the package: libevent..."
	ipkg install $apps_local_space/libevent_*_$pkg_type.ipk 1>$install_log &
	result=`_log_ipkg_install downloadmaster $install_log`
	if [ "$result" == "error" ]; then
		echo "Failed to install libevent!"
		nvram set apps_state_error=4
		exit 1
	else
		rm -f $install_log
	fi
fi

if [ -z "`echo "$list_installed" |grep "ncurses - "`" ]; then
	echo "Installing the package: ncurses..."
	ipkg install $apps_local_space/ncurses_*_$pkg_type.ipk 1>$install_log &
	result=`_log_ipkg_install downloadmaster $install_log`
	if [ "$result" == "error" ]; then
		echo "Failed to install ncurses!"
		nvram set apps_state_error=4
		exit 1
	else
		rm -f $install_log
	fi
fi

if [ -z "`echo "$list_installed" |grep "libxml2 - "`" ]; then
	echo "Installing the package: libxml2..."
	ipkg install $apps_local_space/libxml2_*_$pkg_type.ipk 1>$install_log &
	result=`_log_ipkg_install downloadmaster $install_log`
	if [ "$result" == "error" ]; then
		echo "Failed to install libxml2!"
		nvram set apps_state_error=4
		exit 1
	else
		rm -f $install_log
	fi
fi

if [ -z "$is_arm_machine" ]; then
	if [ -z "`echo "$list_installed" |grep "libuclibc++ - "`" ]; then
		echo "Installing the package: libuclibc++..."
		ipkg install $apps_local_space/libuclibc++_*_$pkg_type.ipk 1>$install_log &
		result=`_log_ipkg_install downloadmaster $install_log`
		if [ "$result" == "error" ]; then
			echo "Failed to install libuclibc++!"
			nvram set apps_state_error=4
			exit 1
		else
			rm -f $install_log
		fi
	fi
fi

if [ -z "`echo "$list_installed" |grep "libsigc++ - "`" ]; then
	echo "Installing the package: libsigc++..."
	ipkg install $apps_local_space/libsigc++_*_$pkg_type.ipk 1>$install_log &
	result=`_log_ipkg_install downloadmaster $install_log`
	if [ "$result" == "error" ]; then
		echo "Failed to install libsigc++!"
		nvram set apps_state_error=4
		exit 1
	else
		rm -f $install_log
	fi
fi

if [ -z "`echo "$list_installed" |grep "libpar2 - "`" ]; then
	echo "Installing the package: libpar2..."
	ipkg install $apps_local_space/libpar2_*_$pkg_type.ipk 1>$install_log &
	result=`_log_ipkg_install downloadmaster $install_log`
	if [ "$result" == "error" ]; then
		echo "Failed to install libpar2!"
		nvram set apps_state_error=4
		exit 1
	else
		rm -f $install_log
	fi
fi

if [ -z "`echo "$list_installed" |grep "pcre - "`" ]; then
	echo "Installing the package: pcre..."
	ipkg install $apps_local_space/pcre_*_$pkg_type.ipk 1>$install_log &
	result=`_log_ipkg_install downloadmaster $install_log`
	if [ "$result" == "error" ]; then
		echo "Failed to install pcre!"
		nvram set apps_state_error=4
		exit 1
	else
		rm -f $install_log
	fi
fi

if [ -z "`echo "$list_installed" |grep "spawn-fcgi - "`" ]; then
	echo "Installing the package: spawn-fcgi..."
	ipkg install $apps_local_space/spawn-fcgi_*_$pkg_type.ipk 1>$install_log &
	result=`_log_ipkg_install downloadmaster $install_log`
	if [ "$result" == "error" ]; then
		echo "Failed to install spawn-fcgi!"
		nvram set apps_state_error=4
		exit 1
	else
		rm -f $install_log
	fi
fi

DM_file=`ls $apps_local_space/downloadmaster_*_$pkg_type.ipk`
if [ -n "$DM_file" ]; then
	if [ -n "$2" ]; then
		file_name=`echo $2 |awk '{FS="/"; print $NF}'`
	else
		file_name=`echo $DM_file |awk '{FS="/"; print $NF}'`
	fi
	file_ver=`echo $file_name |awk '{FS="_"; print $2}'`
	DM_version1=`echo $file_ver |awk '{FS="."; print $1}'`
	DM_version4=`echo $file_ver |awk '{FS="."; print $4}'`
	if [ "$DM_version1" -gt "2" ] && [ "$DM_version4" -gt "59" ]; then
		if [ -z "`echo "$list_installed" |grep "readline - "`" ]; then
			echo "Installing the package: readline..."
			ipkg install $apps_local_space/readline_*_$pkg_type.ipk 1>$install_log &
			result=`_log_ipkg_install downloadmaster $install_log`
			if [ "$result" == "error" ]; then
				echo "Failed to install readline!"
				nvram set apps_state_error=4
				exit 1
			else
				rm -f $install_log
			fi
		fi

		if [ "$DM_version1" -gt "2" ] && [ "$DM_version4" -gt "74" ]; then
			if [ -z "`echo "$list_installed" |grep "expat - "`" ]; then
				echo "Installing the package: expat..."
				ipkg install $apps_local_space/expat_*_$pkg_type.ipk 1>$install_log &
				result=`_log_ipkg_install downloadmaster $install_log`
				if [ "$result" == "error" ]; then
					echo "Failed to install expat!"
					nvram set apps_state_error=4
					exit 1
				else
					rm -f $install_log
				fi
			fi

			if [ -z "`echo "$list_installed" |grep "wxbase - "`" ]; then
				echo "Installing the package: wxbase..."
				ipkg install $apps_local_space/wxbase_*_$pkg_type.ipk 1>$install_log &
				result=`_log_ipkg_install downloadmaster $install_log`
				if [ "$result" == "error" ]; then
					echo "Failed to install wxbase!"
					nvram set apps_state_error=4
					exit 1
				else
					rm -f $install_log
				fi
			fi
		fi
	fi
fi


APPS_MOUNTED_TYPE=`mount |grep "/dev/$APPS_DEV on " |awk '{print $5}'`
if [ "$APPS_MOUNTED_TYPE" == "vfat" ] || [ "$APPS_MOUNTED_TYPE" == "tfat" ] || [ "$APPS_MOUNTED_TYPE" == "texfat" ]; then
	app_move_to_pool.sh $APPS_DEV
	if [ "$?" != "0" ]; then
		# apps_state_error was already set by app_move_to_pool.sh.
		exit 1
	fi
fi

app_base_link.sh
if [ "$?" != "0" ]; then
	# apps_state_error was already set by app_base_link.sh.
	exit 1
fi

nvram set apps_depend_do=
nvram set apps_depend_action=
nvram set apps_depend_action_target=

echo "Success to build the base environment!"

#!/bin/sh
# $1: filesystem type, $2: device path.


ret_dir=/tmp/fsck_ret

# $1: device path, $2: error code.
_set_fsck_code(){
	pool_name=`echo "$1" |awk '{FS="/"; print $NF}'`

	if [ ! -d "$ret_dir" ]; then
		rm -rf $ret_dir
		mkdir $ret_dir
	fi

	rm -f $ret_dir/$pool_name.[0-3]
	touch "$ret_dir/$pool_name.$2"
}

# $1: device path.
_get_fsck_logfile(){
	pool_name=`echo "$1" |awk '{FS="/"; print $NF}'`

	if [ ! -d "$ret_dir" ]; then
		rm -rf $ret_dir
		mkdir $ret_dir
	fi

	echo "$ret_dir/$pool_name.log"
}

if [ -z "$1" ] || [ -z "$2" ]; then
	echo "Usage: app_fsck.sh [filesystem type] [device's path]"
	exit 0
elif [ "$1" == "vfat" ] || [ "$1" == "msdos" ]; then
	_set_fsck_code $2 3
	exit 0
fi

autocheck_option=
autofix_option=
autofix=`nvram get apps_state_autofix`
fs_mod=`nvram get usb_hfs_mod`

log_file=`_get_fsck_logfile $2`
log_option="> $log_file 2>&1"

if [ "$autofix" == "1" ]; then
	if [ "$1" == "ntfs" ]; then
		autocheck_option="-a"
		autofix_option="-f"
	elif [ "$1" == "hfs" ] || [ "$1" == "hfsplus" ] || [ "$1" == "thfsplus" ] || [ "$1" == "hfs+j" ] || [ "$1" == "hfs+jx" ]; then
		if [ "$fs_mod" == "open" ]; then
			autocheck_option=
			autofix_option="-f"
		elif [ "$fs_mod" == "paragon" ]; then
			autocheck_option="-a"
			autofix_option="-f"
		elif [ "$fs_mod" == "tuxera" ]; then
			autocheck_option=
			autofix_option=
		fi
	else
		autocheck_option=
		autofix_option=p
	fi
else
	if [ "$1" == "ntfs" ]; then
		autocheck_option="-a"
		autofix_option=
	elif [ "$1" == "hfs" ] || [ "$1" == "hfsplus" ] || [ "$1" == "thfsplus" ] || [ "$1" == "hfs+j" ] || [ "$1" == "hfs+jx" ]; then
		if [ "$fs_mod" == "open" ]; then
			autocheck_option="-q"
			autofix_option=
		elif [ "$fs_mod" == "paragon" ]; then
			autocheck_option="-a"
			autofix_option=
		elif [ "$fs_mod" == "tuxera" ]; then
			autocheck_option=
			autofix_option=
		fi
	else
		autocheck_option=n
		autofix_option=
	fi
fi

free_caches -w 0
set -o pipefail
_set_fsck_code $2 2
if [ "$1" == "ntfs" ]; then
	c=0
	RET=1
	while [ ${c} -lt 4 -a ${RET} -ne 0 ] ; do
		c=$((${c} + 1))
		eval chkntfs $autocheck_option $autofix_option --verbose $2 $log_option
		RET=$?
		if [ ${RET} -ge 251 -a ${RET} -le 254 ] ; then
			break;
		fi
	done
elif [ "$1" == "hfs" ] || [ "$1" == "hfsplus" ] || [ "$1" == "thfsplus" ] || [ "$1" == "hfs+j" ] || [ "$1" == "hfs+jx" ]; then
	c=0
	RET=1
	while [ ${c} -lt 4 -a ${RET} -ne 0 ] ; do
		c=$((${c} + 1))
		if [ "$fs_mod" == "open" ]; then
			eval fsck.hfsplus $autocheck_option $autofix_option $2 $log_option
		elif [ "$fs_mod" == "paragon" ]; then
			eval chkhfs $autocheck_option $autofix_option --verbose $2 $log_option
		elif [ "$fs_mod" == "tuxera" ]; then
			eval fsck_hfs $autocheck_option $autofix_option $2 $log_option
		fi
		RET=$?
		if [ ${RET} -ge 251 -a ${RET} -le 254 ] ; then
			break;
		fi
	done
else
	eval fsck.$1 -$autocheck_option$autofix_option -v $2 $log_option
	RET=$?
fi
free_caches -w 0 -c 0

if [ "${RET}" != "0" ]; then
	_set_fsck_code $2 1
else
	_set_fsck_code $2 0
fi


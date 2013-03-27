#!/bin/sh
# part of usb_modeswitch 1.2.3
device_in()
{
	if [ ! -e /var/lib/usb_modeswitch/$1 ]; then
		return 0
	fi
	while read line
	do
		if [ $(expr "$line" : "$2:$3") != 0 ]; then
			return 1
		fi
	done </var/lib/usb_modeswitch/$1
	if [ $(expr "$line" : "$2:$3") != 0 ]; then
		return 1
	fi
	return 0
}

if [ $(expr "$1" : "--.*") ]; then
	p_id=$4
	if [ -z $p_id ]; then
		prod=$5
		if [ -z $prod ]; then
			prod=$3
		fi
		prod=${prod%/*}
		v_id=0x${prod%/*}
		p_id=0x${prod#*/}
		if [ "$v_id" = "0x" ]; then
			v_id="0"
			p_id="0"
		fi
		v_id="$(printf %04x $(($v_id)))"
		p_id="$(printf %04x $(($p_id)))"
	else
		v_id=$3
	fi
fi
PATH=/sbin:/usr/sbin:$PATH
case "$1" in
	--driver-bind)
		(
		dir=$(ls -d /sys$2/ttyUSB* 2>/dev/null)
		sleep 1
		if [ ! -z "$dir" ]; then
			exit 0
		fi
		set +e
		device_in "bind_list" $v_id $p_id
		if [ "$?" = "1" ]; then
			id_attr="/sys/bus/usb-serial/drivers/option1/new_id"
			if [ ! -e "$id_attr" ]; then
				modprobe option 2>/dev/null || true
			fi
			if [ -e "$id_attr" ]; then
				echo "$v_id $p_id ff" > $id_attr
			else
				modprobe -r usbserial
				modprobe usbserial "vendor=0x$v_id" "product=0x$p_id"
			fi
		fi
		) &
		exit 0
		;;
	--symlink-name)
		device_in "link_list" $v_id $p_id
		if [ "$?" = "1" ]; then
			if [ -e "/usr/sbin/usb_modeswitch_dispatcher" ]; then
				exec usb_modeswitch_dispatcher $1 $2 2>>/dev/null
			fi
		fi
		exit 0
		;;
esac
exec 1<&- 2<&- 5<&- 7<&-
(
count=120
while [ $count != 0 ]; do
	if [ ! -e "/usr/sbin/usb_modeswitch_dispatcher" ]; then
		sleep 1
		count=$(($count - 1))
	else
		exec usb_modeswitch_dispatcher --switch-mode $1 $0 &
		exit 0
	fi
done
) &
exit 0

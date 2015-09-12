#!/bin/sh
# echo "This is a script to find the modem act TTY nodes out."


modem_act_path=`nvram get usb_modem_act_path`
modem_type=`nvram get usb_modem_act_type`
modem_vid=`nvram get usb_modem_act_vid`
dev_home=/dev

at_lock="flock -x /tmp/at_cmd_lock"


_find_act_devs(){
	act_devs=

	ttyUSB_devs=`cd $dev_home && ls ttyUSB* 2>/dev/null`
	if [ -n "$ttyUSB_devs" ]; then
		tty_devs=`echo $ttyUSB_devs`
	fi
	ttyACM_devs=`cd $dev_home && ls ttyACM* 2>/dev/null`
	if [ -n "$ttyACM_devs" ]; then
		ttyACM_devs=`echo $ttyACM_devs`

		tty_devs=$tty_devs" "$ttyACM_devs
	fi
	tty_devs=`echo $tty_devs |sed 's/$/ /g' |sed 's/ $//g'`

	for dev in $tty_devs; do
		path=`nvram get usb_path_$dev`
		if [ "$path" == "$modem_act_path" ]; then
			if [ -n "$act_devs" ]; then
				act_devs=$act_devs" "$dev
			else
				act_devs=$dev
			fi
		fi
	done

	echo "$act_devs"
}

_find_in_out_devs(){
	io_devs=

	for dev in $1; do
		t=`echo $dev |head -c 6`
		if [ "$t" == "ttyACM" ]; then
			if [ -n "$io_devs" ]; then
				io_devs=$io_devs" "$dev
			else
				io_devs=$dev
			fi

			continue
		fi

		real_path=`readlink -f /sys/class/tty/$dev/device`"/.."
		eps=`cd $real_path && ls -d ep_* 2>/dev/null`
		if [ -z "$eps" ]; then
			continue
		fi

		got_in=0
		got_out=0
		for ep in $eps; do
			direction=`cat $real_path/$ep/direction`
			if [ "$direction" == "in" ]; then
				got_in=1
			elif [ "$direction" == "out" ]; then
				got_out=1
			fi

			if [ $got_in -eq 1 -a $got_out -eq 1 ]; then
				if [ -n "$io_devs" ]; then
					io_devs=$io_devs" "$dev
				else
					io_devs=$dev
				fi

				break
			fi
		done
	done

	echo "$io_devs"
}

_find_dial_devs(){
	dial_devs=

	got_acm=0
	for dev in $1; do
		t=`echo $dev |head -c 6`
		if [ "$t" == "ttyACM" ]; then
			got_acm=1
			break
		fi
	done

	count=0
	for dev in $1; do
		t=`echo $dev |head -c 6`
		if [ $got_acm -eq 1 -a "$t" == "ttyUSB" ]; then
			continue
		fi

		$at_lock chat -t 1 -e '' 'ATQ0' OK >> /dev/$dev < /dev/$dev 2>/dev/null
		if [ "$?" == "0" ]; then
			count=$((count+1))

			if [ -n "$dial_devs" ]; then
				dial_devs=$dial_devs" "$dev
			else
				dial_devs=$dev
			fi
		fi

		if [ $count -eq 2 ]; then
			break
		fi
	done

	echo "$dial_devs"
}

_find_int_devs(){
	int_devs=
	first_dev=

	for dev in $1; do
		if [ -z "$first_dev" ]; then
			first_dev=$dev
		fi

		real_path=`readlink -f /sys/class/tty/$dev/device`"/.."
		eps=`cd $real_path && ls -d ep_* 2>/dev/null`
		if [ -z "$eps" ]; then
			continue
		fi

		for ep in $eps; do
			attribute=`cat $real_path/$ep/bmAttributes`
			if [ "$attribute" == "03" ]; then
				if [ -n "$int_devs" ]; then
					int_devs=$int_devs" "$dev
				else
					int_devs=$dev
				fi

				break
			fi
		done
	done

	if [ -n "$int_devs" ]; then
		echo "$int_devs"
	else
		echo "$first_dev" # default
	fi
}

_find_first_int_dev(){
	int_devs=`_find_int_devs "$1"`

	for dev in $int_devs; do
		echo "$dev"
		break
	done
}

_find_noint_devs(){
	noint_devs=

	for dev in $1; do
		real_path=`readlink -f /sys/class/tty/$dev/device`"/.."
		eps=`cd $real_path && ls -d ep_* 2>/dev/null`
		if [ -z "$eps" ]; then
			continue
		fi

		got_int=0
		for ep in $eps; do
			attribute=`cat $real_path/$ep/bmAttributes`
			if [ "$attribute" == "03" ]; then
				got_int=1

				break
			fi
		done

		if [ $got_int -ne 1 ]; then
			if [ -n "$noint_devs" ]; then
				noint_devs=$noint_devs" "$dev
			else
				noint_devs=$dev
			fi
		fi
	done

	echo "$noint_devs"
}

_except_first_int_dev(){
	first_int_dev=`_find_first_int_dev "$1"`
	bulk_dev=

	for dev in $1; do
		if [ "$dev" != "$first_int_dev" ]; then
			echo "$dev"
			return
		fi
	done
}


act_devs=`_find_act_devs`
echo "act_devs=$act_devs."

io_devs=`_find_in_out_devs "$act_devs"`
echo "io_devs=$io_devs."

if [ "$modem_type" == "tty" ] && [ "$modem_vid" == "6610" -o "$modem_vid" == "1032" ]; then # e.q. ZTE MF637U, ROYAL Q110.
	first_int_dev=`_find_first_int_dev "$io_devs"`
	echo "first_int_dev=$first_int_dev."

	first_bulk_dev=""
	echo "first_bulk_dev=$first_bulk_dev."
else
	dial_devs=`_find_dial_devs "$io_devs"`
	echo "dial_devs=$dial_devs."

	first_int_dev=`_find_first_int_dev "$dial_devs"`
	echo "first_int_dev=$first_int_dev."

	first_bulk_dev=`_except_first_int_dev "$dial_devs"`
	echo "first_bulk_dev=$first_bulk_dev."
fi

nvram set usb_modem_act_int=$first_int_dev
nvram set usb_modem_act_bulk=$first_bulk_dev


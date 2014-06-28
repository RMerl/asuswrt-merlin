#!/bin/sh
echo "This is a script to find the modem type out."


modem_act_path=`nvram get usb_modem_act_path`
node_home=/sys/devices


_find_act_type(){
	home="$node_home/"`cd $node_home && find -name "$1" 2>/dev/null`
	nodes=`cd $home && ls -d $1:* 2>/dev/null`

	got_tty=0
	for node in $nodes; do
		path=`readlink -f $home/$node/driver 2>/dev/null`

		t=${path##*drivers/}
		if [ "$t" == "option" ] || [ "$t" == "usbserial" ] || [ "$t" == "usbserial_generic" ]; then
			got_tty=1
			continue
		elif [ "$t" == "cdc_acm" ] || [ "$t" == "acm" ]; then
			got_tty=1
			continue
		elif [ "$t" == "cdc_ether" ]; then
			echo "ecm"
			break
		elif [ "$t" == "rndis_host" ]; then
			echo "rndis"
			break
		elif [ "$t" == "asix" ]; then
			echo "asix"
			break
		elif [ "$t" == "qmi_wwan" ]; then
			echo "qmi"
			break
		elif [ "$t" == "cdc_ncm" ]; then
			echo "ncm"
			break
		elif [ "$t" == "cdc_mbim" ]; then
			echo "mbim"
			break
		fi
	done

	if [ $got_tty -eq 1 ]; then
		echo "tty"
	else
		echo ""
	fi
}

type=`_find_act_type "$modem_act_path"`
echo "type=$type."

nvram set usb_modem_act_type=$type


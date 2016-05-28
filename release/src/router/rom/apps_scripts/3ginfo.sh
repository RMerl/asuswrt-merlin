#!/bin/sh

echo ">"
cat /proc/bus/usb/devices
echo ">"
lsmod
echo ">"
ifconfig
echo ">"
mount
echo ">"
cat /etc/g3.conf.1
echo ">"
cat /etc/g3.conf.2
echo ">"
cat /tmp/ppp/peers/3g
echo ">"
ls /dev/tty*
echo ">"
nvram get firmver
echo ">"
nvram get buildno
echo ">"
nvram show |grep extendno
echo ">"
echo "dualwan nvram:>"
nvram show |grep ^wans_
echo ">"
echo "wan state:>"
nvram show |grep state |grep wan[01]_
echo ">"
echo "modem nvram:>"
nvram get Dev3G
echo ">"
nvram show |grep ^modem_ |grep -v "modem_pincode="
echo ">"
echo "modem state:>"
nvram show |grep g3state
echo ">"
nvram show |grep g3err
echo ">"
echo "modem act state:>"
str=`nvram get usb_modem_act_path`
echo "usb_modem_act_path=$str"
str=`nvram get usb_modem_act_type`
echo "usb_modem_act_type=$str"
str=`nvram get usb_modem_act_dev`
echo "usb_modem_act_dev=$str"
str=`nvram get usb_modem_act_int`
echo "usb_modem_act_int=$str"
str=`nvram get usb_modem_act_bulk`
echo "usb_modem_act_bulk=$str"
str=`nvram get usb_modem_act_vid`
echo "usb_modem_act_vid=$str"
str=`nvram get usb_modem_act_pid`
echo "usb_modem_act_pid=$str"
str=`nvram get usb_modem_act_sim`
echo "usb_modem_act_sim=$str"
str=`nvram get usb_modem_act_signal`
echo "usb_modem_act_signal=$str"
str=`nvram get usb_modem_act_operation`
echo "usb_modem_act_operation=$str"
str=`nvram get usb_modem_act_imsi |cut -c '1-6'`
echo "usb_modem_act_imsi=$str"
str=`nvram get usb_modem_act_tx`
echo "usb_modem_act_tx=$str"
str=`nvram get usb_modem_act_rx`
echo "usb_modem_act_rx=$str"
str=`nvram get usb_modem_act_hwver`
echo "usb_modem_act_hwver=$str"
str=`nvram get usb_modem_act_band`
echo "usb_modem_act_band=$str"
str=`nvram get usb_modem_act_scanning`
echo "usb_modem_act_scanning=$str"
str=`nvram get usb_modem_act_auth`
echo "usb_modem_act_auth=$str"
str=`nvram get usb_modem_act_auth_pin`
echo "usb_modem_act_auth_pin=$str"
str=`nvram get usb_modem_act_auth_puk`
echo "usb_modem_act_auth_puk=$str"
str=`nvram get usb_modem_act_startsec`
echo "usb_modem_act_startsec=$str"
echo ">"
echo "modem autoapn:>"
nvram show |grep ^usb_modem_auto
echo ">"
echo "real ip detect:>"
nvram show |grep "_ipaddr=" |grep wan[01]_
nvram show |grep real |grep wan[01]_
echo ">"
echo "resolv.conf >"
cat /etc/resolv.conf
echo ">"
echo "udhcpd.conf >"
cat /tmp/udhcpd.conf
echo ">"
echo "show dns nvram >"
nvram show |grep dns |grep wan[01]
echo ">"
echo "syslog>"
cat /tmp/syslog.log |tail -n 50
echo ">"
echo "usblog>"
cat /tmp/usb.log
echo ">"
echo "ps>"
ps


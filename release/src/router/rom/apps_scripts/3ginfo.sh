#!/bin/sh

echo ">"
cat /proc/bus/usb/devices
echo ">"
cat /etc/g3.conf.1
echo ">"
cat /etc/g3.conf.2
echo ">"
cat /tmp/ppp/peers/3g
echo ">"
ls /dev/tty*
echo ">"
ifconfig
echo ">"
lsmod
echo ">"
nvram show|grep firm
echo ">"
nvram get Dev3G
echo ">"
nvram show|grep g3state
echo ">"
nvram show|grep g3err
echo ">"
echo "modem nvram:>"
nvram show|grep modem
echo ">"
echo "resolv.conf >"
cat /etc/resolv.conf
echo ">"
echo "udhcpd.conf >"
cat /tmp/udhcpd.conf
echo ">"
echo "show dns nvram >"
nvram show|grep dns
echo ">"
echo "syslog>"
cat /tmp/syslog.log
echo ">"
echo "usblog>"
cat /tmp/usb.log


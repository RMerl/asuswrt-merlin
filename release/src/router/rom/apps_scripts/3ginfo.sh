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
nvram get certid
echo ">"
nvram get Dev3G
echo ">"
nvram get firmver_sub
echo ">"
nvram show|grep dev_
echo ">"
nvram show|grep EVDO
echo ">"
nvram show|grep g3
echo ">"
nvram show|grep pin
echo ">"
nvram show|grep EVDO
echo "hsdpa nvram:>"
nvram show|grep hsdpa
echo "modem nvram:>"
nvram show|grep modem
echo ">"
nvram show|grep run_sh
echo ">"
nvram show|grep g3state
echo ">"
nvram show|grep g3progress
echo ">"
nvram show|grep err
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


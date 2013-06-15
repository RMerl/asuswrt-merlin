#! /bin/sh
#
# remove SWAT deamon from inetd.conf
#
cp /etc/inetd.conf /etc/inetd.conf.O

if [ $? -ne 0 ]; then exit 1; fi
if [ ! -r /etc/inetd.conf.O -o ! -w /etc/inetd.conf ]; then exit 1; fi

sed -e "/^swat/D" -e "/^#SWAT/D" /etc/inetd.conf.O > /etc/inetd.conf

#
# remove SWAT service port from /etc/services
#
cp /etc/services /etc/services.O

if [ $? -ne 0 ]; then exit 1; fi
if [ ! -r /etc/services.O -o ! -w /etc/services ]; then exit 1; fi

sed -e "/^swat/D" -e "/^#SWAT/D" /etc/services.O > /etc/services

#
# restart inetd to reread config files
#
/etc/killall -HUP inetd

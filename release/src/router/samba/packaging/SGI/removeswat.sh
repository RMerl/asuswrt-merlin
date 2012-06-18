#! /bin/sh
#
# remove SWAT deamon from inetd.conf
#
cp /etc/inetd.conf /etc/inetd.O
sed -e "/^swat/D" -e "/^#SWAT/D" /etc/inetd.O > /etc/inetd.conf

#
# remove SWAT service port from /etc/services
#
cp /etc/services /etc/services.O
sed -e "/^swat/D" -e "/^#SWAT/D" /etc/services.O > /etc/services

#
# restart inetd to reread config files
#
/etc/killall -HUP inetd

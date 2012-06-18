#! /bin/sh
#
# add SWAT deamon to inetd.conf
#
cp /etc/inetd.conf /etc/inetd.O
sed -e "/^swat/D" -e "/^#SWAT/D" /etc/inetd.O > /etc/inetd.conf
echo '#SWAT services' >> /etc/inetd.conf
echo swat stream tcp  nowait  root    /usr/samba/bin/swat swat >> /etc/inetd.conf

#
# add SWAT service port to /etc/services
#
cp /etc/services /etc/services.O
sed -e "/^swat/D" -e "/^#SWAT/D" /etc/services.O > /etc/services
echo '#SWAT services' >> /etc/services
echo 'swat              901/tcp                         # SWAT' >> /etc/services

#
# restart inetd to start SWAT
#
/etc/killall -HUP inetd

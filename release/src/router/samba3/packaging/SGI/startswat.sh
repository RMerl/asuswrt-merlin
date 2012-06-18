#! /bin/sh
#
# add SWAT deamon to inetd.conf
#
cp /etc/inetd.conf /etc/inetd.conf.O

if [ $? -ne 0 ]; then exit 1; fi
if [ ! -r /etc/inetd.conf.O -o ! -w /etc/inetd.conf ]; then exit 1; fi

sed -e "/^swat/D" -e "/^#SWAT/D" /etc/inetd.conf.O > /etc/inetd.conf
echo '#SWAT services' >> /etc/inetd.conf
echo swat stream tcp  nowait  root    /usr/samba/bin/swat swat >> /etc/inetd.conf

#
# add SWAT service port to /etc/services
#
cp /etc/services /etc/services.O

if [ $? -ne 0 ]; then exit 1; fi
if [ ! -r /etc/services.O -o ! -w /etc/services ]; then exit 1; fi

sed -e "/^swat/D" -e "/^#SWAT/D" /etc/services.O > /etc/services
echo '#SWAT services' >> /etc/services
echo 'swat              901/tcp                         # SWAT' >> /etc/services

#
# restart inetd to start SWAT
#
/etc/killall -HUP inetd

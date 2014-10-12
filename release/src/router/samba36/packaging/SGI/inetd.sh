#! /bin/sh
#
# kill any running samba processes
#
/etc/killall smbd nmbd
chkconfig samba off

#
# add SAMBA deamons to inetd.conf
#
cp /etc/inetd.conf /etc/inetd.conf.O

if [ $? -ne 0 ]; then exit 1; fi
if [ ! -r /etc/inetd.conf.O -o ! -w /etc/inetd.conf ]; then exit 1; fi

sed -e "/^netbios/D" -e "/^#SAMBA/D" /etc/inetd.conf.O > /etc/inetd.conf
echo '#SAMBA services' >> /etc/inetd.conf
echo netbios-ssn stream tcp  nowait  root    /usr/samba/bin/smbd smbd  >> /etc/inetd.conf
echo netbios-ns  dgram udp   wait    root    /usr/samba/bin/nmbd nmbd -S >> /etc/inetd.conf

#
# add SAMBA service ports to /etc/services
#
cp /etc/services /etc/services.O

if [ $? -ne 0 ]; then exit 1; fi
if [ ! -r /etc/services.O -o ! -w /etc/services ]; then exit 1; fi

sed -e "/^netbios/D" -e "/^#SAMBA/D" /etc/services.O > /etc/services
echo '#SAMBA services' >> /etc/services
echo 'netbios-ns	137/udp				# SAMBA' >> /etc/services
echo 'netbios-ssn	139/tcp				# SAMBA' >> /etc/services

#
# restart inetd to start SAMBA
#
/etc/killall -HUP inetd

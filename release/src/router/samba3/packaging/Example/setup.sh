#!/bin/sh
#
# Note: This file MUST be edited to suit the target OS environment.
#

echo "Setting up for SWAT - The Samba Web Administration Tool"

echo 'swat		901/tcp' >> /etc/services
uniq /etc/services /tmp/tempserv
cp /tmp/tempserv /etc/services
rm /tmp/tempserv
echo 'swat	stream	tcp	nowait.400	root	/usr/local/samba/bin/swat swat' >> /etc/inetd.conf
uniq /etc/inetd.conf /tmp/tempinetd
cp /tmp/tempinetd /etc/inetd.conf
rm /tmp/tempinetd
echo "Creating Symbolic Links for Start up Scripts"
cp -f samba.init /sbin/init.d
chown bin.bin /sbin/init.d/samba.init
chmod 750 /sbin/init.d/samba.init
ln -sf /sbin/init.d/samba.init /sbin/rc0.d/K01samba
ln -sf /sbin/init.d/samba.init /sbin/rc2.d/K91samba
ln -sf /sbin/init.d/samba.init /sbin/rc3.d/S91samba
echo "Done. Now settting up samba command"
ln /sbin/init.d/samba.init /sbin/samba
echo "Done."
echo "To start / stop samba:"
echo "	execute:  samba [start | stop]"

#! /bin/sh

echo
echo
echo Samba has been installed on your system.
echo 
echo Your /etc/services and /etc/inetd.conf files have
echo been modified to automatically start the
echo Samba Web Administration Tool \(SWAT\) when you 
echo connect with a web browser to
echo 
echo http://`hostname`:901
echo 
echo The original versions of /etc/services and
echo /etc/inetd.conf were saved with a .O extension.
echo
echo If you do not wish SWAT to be enabled you may
echo run the script /usr/samba/scripts/removeswat.sh
echo which will remove the entries from /etc/services
echo and /etc/inetd.conf
echo 
echo Please review your configuration settings by
echo connecting to SWAT or editing the file
echo /usr/samba/lib/smb.conf and then starting
echo the smbd and nmbd daemons to complete the 
echo installation. You may start the daemons from
echo the SWAT "Status" page or by executing the
echo following command as root.
echo 
echo /etc/init.d/samba start
echo

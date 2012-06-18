#
# spec file for package samba (Version 1.9.18p1)
# 
# Copyright  (c)  1997  S.u.S.E. GmbH  Fuerth, Germany.
#
# please send bugfixes or comments to feedback@suse.de.
#

Vendor:       S.u.S.E. GmbH, Fuerth, Germany
Distribution: S.u.S.E. Linux 5.1 (i386)
Name:         samba
Release:      1
Copyright:    1992-98 Andrew Tridgell, Karl Auer, Jeremy Allison
Group: 
Provides:     samba smbfs 
Requires: 
Conflicts:    
Autoreqprov:  on
Packager:     feedback@suse.de

Version:      1.9.18p5
Summary:      Samba  is a file server for Unix, similar to LanManager.
Source: samba-1.9.18p5.tar.gz
Source1: smbfs-2.0.2.tar.gz
Patch: samba-1.9.18p5.dif
Patch1: smbfs-2.0.2.dif
%prep
%setup
%patch
%setup -T -n smbfs-2.0.2 -b1
%patch -P 1
%build
cd ../samba-1.9.18p5
make -f Makefile.Linux compile
cd ../smbfs-2.0.2
make -f Makefile.Linux compile
%install
cd ../samba-1.9.18p5
make -f Makefile.Linux install
cd ../smbfs-2.0.2
make -f Makefile.Linux install
Check
%post
echo "Updating etc/rc.config..."
if [ -x bin/fillup ] ; then
  bin/fillup -q -d = etc/rc.config var/adm/fillup-templates/rc.config.samba
else
  echo "ERROR: fillup not found. This should not happen. Please compare"
  echo "etc/rc.config and var/adm/fillup-templates/rc.config.samba and"
  echo "update by hand."
fi
%files
%docdir /usr/doc/packages/samba
/usr/doc/packages/samba
%config /etc/smb.conf
/usr/lib/samba/codepages
/sbin/init.d/rc2.d/K20smb
/sbin/init.d/rc2.d/S20smb
/sbin/init.d/rc3.d/K20smb
/sbin/init.d/rc3.d/S20smb
%config /sbin/init.d/smb
/usr/bin/addtosmbpass
/usr/bin/mksmbpasswd.sh
/usr/bin/make_printerdef
/usr/bin/make_smbcodepage
/usr/bin/nmblookup
/usr/bin/smbclient
/usr/bin/smbmount
/usr/bin/smbpasswd
/usr/bin/smbstatus
/usr/bin/smbtar
/usr/bin/smbumount
/usr/bin/testparm
/usr/bin/testprns
%doc /usr/man/man1/smbclient.1.gz
%doc /usr/man/man1/smbrun.1.gz
%doc /usr/man/man1/smbstatus.1.gz
%doc /usr/man/man1/smbtar.1.gz
%doc /usr/man/man1/testparm.1.gz
%doc /usr/man/man1/testprns.1.gz
%doc /usr/man/man1/make_smbcodepage.1.gz
%doc /usr/man/man5/smb.conf.5.gz
%doc /usr/man/man7/samba.7.gz
%doc /usr/man/man8/nmbd.8.gz
%doc /usr/man/man8/smbd.8.gz
%doc /usr/man/man8/smbmount.8.gz
%doc /usr/man/man8/smbumount.8.gz
%doc /usr/man/man8/smbmnt.8.gz
%doc /usr/man/man8/smbpasswd.8.gz
/usr/sbin/nmbd
/usr/sbin/smbd
/var/adm/fillup-templates/rc.config.samba
%description
Samba is a suite of programs which work together to allow clients to
access Unix filespace and printers via the SMB protocol (Seerver Message
Block). 
CAUTION: The samba daemons are started by the init script
/sbin/init.d/samba, not by inetd. The entries for /usr/sbin/smbd
and /usr/sbin/nmbd must be commented out in /etc/inetd.conf.
In practice, this means that you can redirect disks and printers to
Unix disks and printers from LAN Manager clients, Windows for
Workgroups 3.11 clients, Windows'95 clients, Windows NT clients
and OS/2 clients. There is
also a Unix client program supplied as part of the suite which allows
Unix users to use an ftp-like interface to access filespace and
printers on any other SMB server.
Samba includes the following programs (in summary):
* smbd, the SMB server. This handles actual connections from clients.
* nmbd, the Netbios name server, which helps clients locate servers.
* smbclient, the Unix-hosted client program.
* testprns, a program to test server access to printers.
* testparm, a program to test the Samba configuration file for correctness.
* smb.conf, the Samba configuration file.
* smbprint, a sample script to allow a Unix host to use smbclient
to print to an SMB server.
The suite is supplied with full source and is GPLed.
This package expects its config file under /etc/smb.conf .
Documentation: /usr/doc/packages/samba


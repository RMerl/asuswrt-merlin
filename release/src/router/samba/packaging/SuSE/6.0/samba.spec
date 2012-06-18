#
# spec file for package samba (Version 1.9.18p10)
# 
# Copyright  (c)  1998  SuSE GmbH  Nuernberg, Germany.
#
# please send bugfixes or comments to feedback@suse.de.
#

# neededforbuild  automake

Vendor:       SuSE GmbH, Nuernberg, Germany
Distribution: SuSE Linux 6.0 (i386)
Name:         samba
Release:      17
Copyright:    1992-95 Andrew Tridgell, Karl Auer, Jeremy Allison
Group:        unsorted
Provides:     samba smbfs 


Autoreqprov:  on
Packager:     feedback@suse.de

Version:      2.0.0
Summary:      Samba  is a file server for Unix, similar to LanManager.
Source: samba-2.0.0.tar.gz
Source1: smbfs-2.0.2.tar.gz
Source2: smbfs-2.1.1.tgz
Patch: samba-2.0.0.dif
Patch1: smbfs-2.0.2.dif
Patch2: smbfs-2.1.0.dif
%prep
rm -rf smbfs-2.0.2 smbfs-2.1.0
%setup -b1 -b2
%patch
cd ../smbfs-2.0.2
%patch -P 1
cd ../smbfs-2.1.0
%patch -P 2
%build
case `/usr/share/automake/config.guess` in
  i?86-pc-linux-gnu)
	export EXTRA_LIBS="-lcrypt"
	;;
  *)
	export EXTRA_LIBS=""
	;;
esac
make -f Makefile.Linux compile
cd ../smbfs-2.0.2
make -f Makefile.Linux compile
%install
make -f Makefile.Linux install
cd ../smbfs-2.0.2
make -f Makefile.Linux install
ln -sf ../../sbin/init.d/smb /usr/sbin/rcsmb
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
if grep -q '^[#[:space:]]*swat' etc/inetd.conf ; then
   echo /etc/inetd.conf is up to date
else
   echo updating inetd.conf
   cat >> etc/inetd.conf << EOF

# swat is the Samba Web Administration Tool
# swat    stream  tcp     nowait.400  root    /usr/local/samba/bin/swat swat
EOF
fi
if grep -q '^swat' etc/services ; then
   echo /etc/services is up to date
else
   echo updating services
   cat >> etc/services << EOF
swat            901/tcp		# swat is the Samba Web Administration Tool
EOF
fi
%files
%docdir /usr/doc/packages/samba
/usr/doc/packages/samba
%config /etc/smb.conf
%config /etc/lmhosts
%config /etc/smbpasswd
/usr/lib/samba
/sbin/init.d/rc2.d/K20smb
/sbin/init.d/rc2.d/S20smb
/sbin/init.d/rc3.d/K20smb
/sbin/init.d/rc3.d/S20smb
%config /sbin/init.d/smb
/usr/bin/smbclient
/usr/bin/testparm
/usr/bin/testprns
/usr/bin/smbstatus
/usr/bin/rpcclient
/usr/bin/smbpasswd
/usr/bin/make_smbcodepage
/usr/bin/nmblookup
/usr/bin/make_printerdef
/usr/bin/smbtar
/usr/bin/addtosmbpass
/usr/bin/convert_smbpasswd
/usr/bin/smbmount
/usr/bin/smbumount
%doc /usr/man/man5/lmhosts.5.gz
%doc /usr/man/man1/make_smbcodepage.1.gz
%doc /usr/man/man8/nmbd.8.gz
%doc /usr/man/man1/nmblookup.1.gz
%doc /usr/man/man7/samba.7.gz
%doc /usr/man/man5/smb.conf.5.gz
%doc /usr/man/man1/smbclient.1.gz
%doc /usr/man/man8/smbd.8.gz
%doc /usr/man/man8/smbmnt.8.gz
%doc /usr/man/man8/smbmount.8.gz
%doc /usr/man/man5/smbpasswd.5.gz
%doc /usr/man/man8/smbpasswd.8.gz
%doc /usr/man/man1/smbrun.1.gz
%doc /usr/man/man1/smbstatus.1.gz
%doc /usr/man/man1/smbtar.1.gz
%doc /usr/man/man8/smbumount.8.gz
%doc /usr/man/man8/swat.8.gz
%doc /usr/man/man1/testparm.1.gz
%doc /usr/man/man1/testprns.1.gz
/usr/sbin/nmbd
/usr/sbin/smbd
/usr/sbin/swat
/usr/sbin/mksmbpasswd.sh
/usr/sbin/updatesmbpasswd.sh
/usr/sbin/rcsmb
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

Authors:
--------
    Andrew Tridgell <Andrew.Tridgell@anu.edu.au>
    Karl Auer <Karl.Auer@anu.edu.au>
    Jeremy Allison <jeremy@netcom.com>



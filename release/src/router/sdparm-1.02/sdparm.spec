%define	name	sdparm
%define	version	1.02
%define	release	1

Summary:	List or change SCSI disk parameters
Name:		%{name}
Version:	%{version}
Release:	%{release}
License:	FreeBSD
Group:		Utilities/System
URL:		http://www.torque.net/sg/sdparm.html
Source0:	http://www.torque.net/sg/p/%{name}-%{version}.tgz
BuildRoot:	%{_tmppath}/%{name}-%{version}-root
Packager:	Douglas Gilbert <dgilbert at interlog dot com>

%description
SCSI disk parameters are held in mode pages. This utility lists or
changes those parameters. Other SCSI devices (or devices that use
the SCSI command set) such as CD/DVD and tape drives may also find
parts of sdparm useful. Requires the linux kernel 2.4 series or later.
In the 2.6 series any device node the understands a SCSI command set
may be used (e.g. /dev/sda). In the 2.4 series SCSI device node may be used.

Fetches Vital Product Data pages. Can send commands to start or stop
the media and load or unload removable media.

Warning: It is possible (but unlikely) to change SCSI disk settings
such that the disk stops operating or is slowed down. Use with care.

%prep

%setup -q

%build

# ./autogen.sh --prefix=%{_prefix} --mandir=%{_mandir}
./configure --prefix=%{_prefix} --mandir=%{_mandir}

%install
[ "%{buildroot}" != "/" ] && rm -rf %{buildroot}

make install \
	DESTDIR=$RPM_BUILD_ROOT

%post

%postun

%clean
[ "%{buildroot}" != "/" ] && rm -rf %{buildroot}

%files
%defattr(-,root,root)
%doc ChangeLog INSTALL README CREDITS AUTHORS COPYING notes.txt
%attr(0755,root,root) %{_bindir}/*
# >> should that be %attr(0755,root,root) %{_sbindir}/*   ??
%{_mandir}/man8/*

%changelog
* Mon Oct 08 2007 - dgilbert at interlog dot com
- add block device characteristics VPD page, descriptor based mpages
  * sdparm-1.02
* Thu Apr 05 2007 - dgilbert at interlog dot com
- add element address assignment mode page (smc)
  * sdparm-1.01
* Mon Oct 16 2006 - dgilbert at interlog dot com
- update Background control mode subpage, vendor specific mode pages
  * sdparm-1.00
* Sat Jul 08 2006 - dgilbert at interlog dot com
- add old power condition page for disks
  * sdparm-0.99
* Thu May 18 2006 - dgilbert at interlog dot com
- add medium configuration mode page
  * sdparm-0.98
* Wed Jan 25 2006 - dgilbert at interlog dot com
- add SAT pATA control and medium partition mode (sub)pages
  * sdparm-0.97
* Fri Nov 18 2005 - dgilbert at interlog dot com
- add capacity, ready and sync commands
  * sdparm-0.96
* Tue Sep 20 2005 - dgilbert at interlog dot com
- add debian build directory, decode more VPD pages
  * sdparm-0.95
* Thu Jul 28 2005 - dgilbert at interlog dot com
- add '--command=<cmd>' option
  * sdparm-0.94
* Thu Jun 02 2005 - dgilbert at interlog dot com
- add '--transport=' and '--dbd' options
  * sdparm-0.93
* Fri May 20 2005 - dgilbert at interlog dot com
- add some tape, cd/dvd, disk, ses and rbc mode pages
  * sdparm-0.92
* Fri May 06 2005 - dgilbert at interlog dot com
- if lk 2.4 detected, map non-sg SCSI device node to sg equivalent
  * sdparm-0.91
* Mon Apr 18 2005 - dgilbert at interlog dot com
- initial version
  * sdparm-0.90

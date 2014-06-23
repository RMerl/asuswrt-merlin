Summary: RSTP daemon
Name: rstp
Version: %{VERSION}
Release: %{BUILD}
License: LGPL, GPL
URL: http://www.rainfinity.com/
Vendor: Rainfinity
Packager: Rainfinity
Source0: rstp-%{VERSION}.tgz
Group: Network/Admin
#Requires: bridge-utils = 1.0.4-4


%description
Bridge init script for RainStorage

%prep

%setup -q

%build
make

%install
make install INSTALLPREFIX=$RPM_BUILD_ROOT 

%pre

%post

%preun

%postun

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/sbin
/usr/share/man/man8

%changelog
* Mon Oct 30 2006 Srinivas Aji <Aji_Srinivas@emc.com>
- version 0.16, Don't define STRONGLY_SPEC_802_1_W, not helping much
- Fix logging to go through syslog

* Fri Oct 20 2006 Srinivas Aji <Aji_Srinivas@emc.com>
- version 0.15, fixes based on second round of UNH testing, 802.1w
- rstpctl syntax changes, list all bridges, ports of a bridge

* Wed Sep 13 2006 Srinivas Aji <Aji_Srinivas@emc.com>
- version 0.14, small fixes. fix an fd leak.

* Tue Sep  5 2006 Srinivas Aji <Aji_Srinivas@emc.com>
- version 0.13, Use netlink patch to send/recv BPDUs, instead of LLC
- Some fixes based on first round of UNH testing

* Sun Aug 20 2006 Srinivas Aji <Aji_Srinivas@emc.com>
- version 0.12, some bugfixes, added man pages.

* Thu Aug 10 2006 Srinivas Aji <Aji_Srinivas@emc.com>
- Initial build.
